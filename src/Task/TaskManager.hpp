#pragma once
#include <KPFoundation.hpp>
#include <KPSubject.hpp>
#include <KPDataStoreInterface.hpp>

#include <Task/Task.hpp>
#include <Task/TaskObserver.hpp>
#include <Application/Config.hpp>

#include <vector>
#include "SD.h"

/**
 * @brief Manages and keeping track of tasks
 * 
 */
class TaskManager : public KPComponent,
					public JsonEncodable,
					public Printable,
					public KPSubject<TaskObserver> {
private:
	std::unordered_map<int, Task> tasks;

public:
	// std::vector<Task> tasks;
	const char * taskFolder = nullptr;

	TaskManager()
		: KPComponent("TaskManager") {}

	void init(Config & config) {
		taskFolder = config.taskFolder;
	}

	Task createTask() {
		Task task;
		task.id		  = random(1, RAND_MAX);
		task.schedule = now() - 1;
		return task;
	}

	const std::unordered_map<int, Task> & taskCollection() const {
		return tasks;
	}

	bool setTaskStatus(int id, TaskStatus status) {
		if (tasks.find(id) == tasks.end()) {
			return false;
		}

		tasks[id].status = status;
		updateObservers(&TaskObserver::taskDidUpdate, tasks[id]);
	}

	int numberOfActiveTasks() {
		return std::count_if(tasks.begin(), tasks.end(), [](const std::pair<int, Task> & kv) {
			return kv.second.status == TaskStatus::active;
		});
	}

	bool markTaskAsCompleted(int id) {
		if (tasks.find(id) == tasks.end()) {
			return false;
		}

		tasks[id].status = TaskStatus::completed;
		return true;
	}

	bool removeTask(int id) {
		return tasks.erase(id);
	}

	int removeIf(std::function<bool(const Task &)> predicate) {
		int oldSize = tasks.size();
		for (auto it = tasks.begin(); it != tasks.end();) {
			if (predicate(it->second)) {
				it = tasks.erase(it);
			} else {
				it++;
			}
		}
		return oldSize - tasks.size();
	}

	/** ────────────────────────────────────────────────────────────────────────────
	 *  @brief Load all tasks object from the specified directory in the SD card
	 *  
	 *  @param _dir 
	 *  ──────────────────────────────────────────────────────────────────────────── */
	void loadTasksFromDirectory(const char * _dir = nullptr) {
		const char * dir = _dir ? _dir : taskFolder;

		JsonFileLoader loader;
		loader.createDirectoryIfNeeded(dir);

		// Load task index file and get the number of tasks
		KPStringBuilder<32> indexFilepath(dir, "/index.js");
		StaticJsonDocument<100> indexFile;
		loader.load(indexFilepath, indexFile);

		// Decode each task object into memory
		int count = indexFile["count"];
		for (int i = 0; i < count; i++) {
			KPStringBuilder<32> filepath(dir, "/task-", i, ".js");
			Task task;
			loader.load(filepath, task);
			insert(task, false);  // All tasks in the SD card should be unique
		}
	}

	// Performe identity check and update the task
	// int updateTaskWithData(JsonDocument & data, JsonDocument & response) {
	// 	serializeJsonPretty(data, Serial);

	// 	using namespace JsonKeys;
	// 	int index = findTaskWithName(data[TASK_NAME]);
	// 	if (index == -1) {
	// 		response["error"] = "No task with such name";
	// 		return -1;
	// 	}

	// 	// Check there is a task with newName then
	// 	// replaces the name with new name if none is found
	// 	if (data.containsKey(TASK_NEW_NAME)) {
	// 		if (findTaskWithName(data[TASK_NEW_NAME]) == -1) {
	// 			data[TASK_NAME] = data[TASK_NEW_NAME];
	// 		} else {
	// 			KPStringBuilder<64> error("Task with name ", data[TASK_NEW_NAME].as<char *>(), " already exist");
	// 			response["error"] = (char *) error.c_str();
	// 			return -1;
	// 		}
	// 	}

	// 	JsonVariant payload = response.createNestedObject("payload");
	// 	tasks[index]		= Task(data.as<JsonObject>());
	// 	tasks[index].encodeJSON(payload);

	// 	KPStringBuilder<100> success("Saved", data[TASK_NAME].as<char *>());
	// 	response["success"] = (char *) success.c_str();
	// 	return index;
	// }

	/** ────────────────────────────────────────────────────────────────────────────
	 *  @brief Get the Active Task Ids sorted by their schedules (<)
	 *  
	 *  @return std::vector<int> list of ids
	 *  ──────────────────────────────────────────────────────────────────────────── */
	std::vector<int> getActiveSortedTaskIds() {
		std::vector<int> result;
		result.reserve(tasks.size());

		for (const auto & kv : tasks) {
			if (kv.second.status == TaskStatus::active) {
				result.push_back(kv.first);
			}
		}

		std::sort(result.begin(), result.end(), [this](int a, int b) {
			return tasks[a].schedule < tasks[b].schedule;
		});

		return result;
	}

	/** ────────────────────────────────────────────────────────────────────────────
	 *  @brief Insert task into TaskManager's internal data structure 
	 *  
	 *  @param task Task object to be inserted 
	 *  @param forced Forced ID generation if task.id already exists
	 *  @return bool true on successful insertion, false otherwise
	 *  ──────────────────────────────────────────────────────────────────────────── */
	bool insert(Task & task, bool forced = false) {
		if (forced) {
			while (tasks.insert({task.id, task}).second == false) {
				task.id = random(RAND_MAX);
			}

			return true;
		}

		if (tasks.find(task.id) == tasks.end()) {
			return false;
		}

		tasks.insert({task.id, task});
		writeTaskArrayToDirectory();
		return true;
	}

	/** ────────────────────────────────────────────────────────────────────────────
	 *  @brief Update the index file containing info about tasks
	 *  
	 *  @param _dir Path to tasks directory (default=~/tasks)
	 *  ──────────────────────────────────────────────────────────────────────────── */
	void updateIndexFile(const char * _dir = nullptr) {
		const char * dir = _dir ? _dir : taskFolder;

		JsonFileLoader loader;
		loader.createDirectoryIfNeeded(dir);

		KPStringBuilder<32> indexFilepath(dir, "/index.js");
		StaticJsonDocument<100> indexJson;
		indexJson["count"] = tasks.size();
		loader.save(indexFilepath, indexJson);
	}

	/** ────────────────────────────────────────────────────────────────────────────
	 *  @brief Write task array to SD directory
	 *  
	 *  @param _dir Path to tasks directory (default=~/tasks)
	 *  ──────────────────────────────────────────────────────────────────────────── */
	void writeTaskArrayToDirectory(const char * _dir = nullptr) {
		const char * dir = _dir ? _dir : taskFolder;

		JsonFileLoader loader;
		loader.createDirectoryIfNeeded(dir);

		for (size_t i = 0; i < tasks.size(); i++) {
			KPStringBuilder<32> filename("task-", i, ".js");
			KPStringBuilder<64> filepath(dir, "/", filename);
			loader.save(filepath, tasks[i]);
		}

		updateIndexFile(dir);
	}

#pragma region JSONENCODABLE
	static const char * encoderName() {
		return "TaskManager";
	}

	static constexpr size_t encoderSize() {
		return Task::encoderSize() * 5;
	}

	bool encodeJSON(const JsonVariant & dst) const override {
		for (const auto & kv : tasks) {
			JsonVariant obj = dst.createNestedObject();
			if (!kv.second.encodeJSON(obj)) {
				return false;
			}
		}

		return true;
	}
#pragma endregion
#pragma region PRINTABLE

	size_t printTo(Print & p) const {
		size_t charWritten = 0;
		charWritten += p.println("[");
		for (const auto & kv : tasks) {
			charWritten += p.print(kv.second);
			charWritten += p.println(",");
		}

		return charWritten + p.println("]");
	}
#pragma endregion
};
