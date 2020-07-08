#pragma once
#include <KPFoundation.hpp>
#include <KPSubject.hpp>
#include <Components/SensorArrayObserver.hpp>

#include <SSC.h>

#define PSAddr 0x08
#define FSAddr 0x07
#define BSAddr 0x77
#define DSAddr 0x76

inline bool checkForConnection(unsigned char addr) {
	Wire.begin();
	Wire.requestFrom(addr, 1);
	return Wire.read() != -1;
}

// The Curiously Recurring Template Pattern (CRTP) allows us to build functionalities
// without dynamic dispatch.
// See: https://www.fluentcpp.com/2017/05/12/curiously-recurring-template-pattern/

template <typename T>
class Crtp {
protected:
	T & underlying() {
		return static_cast<T &>(*this);
	}
	T const & underlying() const {
		return static_cast<T const &>(*this);
	}
};

template <typename SensorType>
class I2CSensor : public Crtp<SensorType> {
protected:
	friend SensorType;
	I2CSensor() {}

public:
	unsigned char enabled			  = false;
	unsigned char updated			  = false;
	unsigned long timeSinceLastUpdate = 0;
	float updateFreqHz				  = 2;

	void updateSensor() {
		if (!enabled) {
			return;
		}

		if ((millis() - timeSinceLastUpdate) >= 1000 / updateFreqHz) {
			this->underlying().update();
			updated				= true;
			timeSinceLastUpdate = millis();
		}
	}

	bool didUpdate() {
		if (updated) {
			updated = false;
			return true;
		} else {
			return false;
		}
	}
};

class PressureSensor : public I2CSensor<PressureSensor> {
	SSC ps{PSAddr};
	PressureSensor(PressureSensor & rhs) = delete;

public:
	void setup() {
		if (checkForConnection(PSAddr)) {
			enabled = true;
		}

		ps.setMinRaw(1638);
		ps.setMaxRaw(14745);
		ps.setMinPressure(0);
		ps.setMaxPressure(30);
		ps.start();
	};

	void update() {
		ps.update();
	}

public:
	std::pair<float, float> getValue() {
		return {ps.pressure(), ps.temperature()};
	}
};

class SensorArray : public KPComponent, public KPSubject<SensorArrayObserver> {
public:
	using KPComponent::KPComponent;
	PressureSensor ps;

	void setup() override {
		ps.setup();
		ps.updateFreqHz = 2;
		println(ps.enabled ? GREEN("Pressure sensor connected")
						   : RED("Pressure sensor not connected"));
	}

	void updatePs() {
		if (ps.enabled == false) {
			return;
		}

		if (ps.didUpdate()) {
			updateObservers(&SensorArrayObserver::pressureSensorDidUpdate, ps.getValue());
		}

		ps.updateSensor();
	}

	void update() override {
		updatePs();
	}
};