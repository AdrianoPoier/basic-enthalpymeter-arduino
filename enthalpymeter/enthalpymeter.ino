#include <GyverNTC.h>

class Routine{
  unsigned long waitTime, autoUpdateTime;
  struct{
    bool
    autoUpdate: 1,
    pause: 1;
  };

  public: // constructor
  Routine(unsigned long _autoUpdateTime = 0, bool _pause = false){
    waitTime = 0;
    autoUpdateTime = _autoUpdateTime;
    if(autoUpdateTime == 0) autoUpdate = false;
    else autoUpdate = true;
    pause = _pause;
  }

  protected: // functions for access in inheritances
  void routine_delay(uint32_t ms){
    waitTime = millis() + ms;
  }

  virtual void update(){}

  public:
  void routine_update(){
    if(millis() > waitTime && !pause){
      this->update();
      if(autoUpdate)
        routine_delay(autoUpdateTime);
    }
  }

  void routine_pause(){pause = true;}
  void routine_resume(){pause = false;}
};

template <byte s = 10>
class Temp : public Routine{
  GyverNTC therm;
  float samples[s];
  byte samplesCursor;
  byte missingSamples;
  float samplesFluctuation;
  bool newSamples;

  public: // constructor
  Temp(byte pin, uint32_t _sampleTime): Routine(_sampleTime), therm(pin, 10000, 3435) {
    clear();
  }

  protected:
  virtual void update(){
    samplesCursor = (samplesCursor+1) % s;
    samples[samplesCursor] = therm.getTempAverage(5);
    newSamples = true;
    if(missingSamples > 0) missingSamples--;
  }

  public:
  float fluctuation(){
    if(newSamples && missingSamples == 0){
      float top = samples[0], bottom = samples[0];
      for(byte i=1; i < s; i++){
        if(samples[i] < bottom) bottom = samples[i];
        if(samples[i] > top) top = samples[i];
      }
      samplesFluctuation = top - bottom;
      newSamples = false;
    }
    return samplesFluctuation;
  }

  void clear(){
    for(samplesCursor = 0; samplesCursor < s; samplesCursor++)
      samples[samplesCursor] = 0;
    samplesCursor = 0;
    missingSamples = s;
    samplesFluctuation = 0;
    newSamples = false;
  }

  bool getNewSamples(){return newSamples;}
  byte getMissingSamples(){return missingSamples;}
  float getTemp(){return samples[samplesCursor];}
};

class GPIO_Button : public Routine{
  byte pin;
  unsigned long pressValidation;
  struct{
    bool
    lastInput: 1,
    pressed: 1,
    click: 1,
    hadClick: 1;
  };

  public:
  GPIO_Button(byte _pin, uint32_t _updateTime): Routine(_updateTime) {
    pin = _pin;
    pressValidation = 0;
    lastInput = false;
    pressed = false;
    click = false;
    hadClick = false;
    pinMode(pin, INPUT);
  }

  bool getClick(){bool aux = click; click = false; return aux;}

  protected:
  virtual void update(){
    bool input = digitalRead(pin);
    if(input == lastInput){
      if(millis() > pressValidation){
        pressed = input;
        if(!hadClick && pressed){
          click = true;
          hadClick = true;
        }else if(!pressed) hadClick = false;
      }
    }else{
      lastInput = input;
      pressValidation = millis() + 100; // <-- validation time
    }
  }
};

class LED{
  public:
  enum LedState{
    OFF,
    ON
  };

  private:
  byte pin;
  bool onPolarity, state;

  public:
  LED(byte _pin, bool _onPolarity = HIGH, LedState initialState = OFF){
    pin = _pin;
    onPolarity = _onPolarity;
    pinMode(pin, OUTPUT);
    setState(initialState);
  }

  void setState(LedState _state){
    state = (onPolarity) ? _state : !_state;
    digitalWrite(pin, state);
  }
};

namespace CalibrationSteps{
  enum CalibrationSteps{
    measureInternalTemp,
    measuringInternalTemp,
    measureExternalTemp,
    measuringExternalTemp,
    measureFinalTemp,
    measuringFinalTemp,
    calibrated
  };
}

namespace MeasurementSteps{
  enum MeasurementSteps{
    measureInitialTemp,
    measuringInitialTemp,
    measureFinalTemp,
    measuringFinalTemp,
    finished
  };
}

class Enthalpy : public Routine{
  Temp<> &cupProbe;
  Temp<> &extProbe;
  GPIO_Button &button;
  LED &stabilityIndicatorLED;
  CalibrationSteps::CalibrationSteps calibrationStep;
  MeasurementSteps::MeasurementSteps measurementStep;
  float initTemp, finalTemp, extTemp;
  float hotWaterCapacity;
  float internalThermalCapacity;
  float measuredEnthalpy;

  public:
  Enthalpy(float _hotWaterCapacity, Temp<> &_cupProbe, Temp<> &_extProbe, GPIO_Button &_button, LED &_stabilityIndicatorLED):
    Routine(50), cupProbe(_cupProbe), extProbe(_extProbe), button(_button), stabilityIndicatorLED(_stabilityIndicatorLED) {
    calibrationStep = 0;
    measurementStep = MeasurementSteps::finished;
    initTemp = 0; finalTemp = 0; extTemp = 0;
    hotWaterCapacity = _hotWaterCapacity;
    internalThermalCapacity = 0;
    measuredEnthalpy = 0;
  }

  protected:
  void calibration(){
    switch(calibrationStep){
      case CalibrationSteps::measureInternalTemp:
        cupProbe.clear();
        cupProbe.routine_resume();
        calibrationStep = CalibrationSteps::measuringInternalTemp;
        break;

      case CalibrationSteps::measuringInternalTemp:
        if(cupProbe.getNewSamples()){
          Serial.println(cupProbe.getTemp());
          if(cupProbe.getMissingSamples() == 0){
            if(cupProbe.fluctuation() < 1){
              stabilityIndicatorLED.setState(LED::ON);
              if(button.getClick()){
                initTemp = cupProbe.getTemp();
                stabilityIndicatorLED.setState(LED::OFF);
                calibrationStep = CalibrationSteps::measureExternalTemp;
              }
            }else stabilityIndicatorLED.setState(LED::OFF);
          }
        }
        break;

      case CalibrationSteps::measureExternalTemp:
        extProbe.clear();
        extProbe.routine_resume();
        calibrationStep = CalibrationSteps::measuringExternalTemp;
        break;

      case CalibrationSteps::measuringExternalTemp:
        if(extProbe.getNewSamples()){
          Serial.println(extProbe.getTemp());
          if(extProbe.getMissingSamples() == 0){
            if(extProbe.fluctuation() < 1){
              stabilityIndicatorLED.setState(LED::ON);
              if(button.getClick()){
                extTemp = extProbe.getTemp();
                stabilityIndicatorLED.setState(LED::OFF);
                extProbe.routine_pause();
                calibrationStep = CalibrationSteps::measureFinalTemp;
              }
            }else stabilityIndicatorLED.setState(LED::OFF);
          }
        }
        break;
      
      case CalibrationSteps::measureFinalTemp:
        cupProbe.clear();
        cupProbe.routine_resume();
        calibrationStep = CalibrationSteps::measuringFinalTemp;
        break;

      case CalibrationSteps::measuringFinalTemp:
        if(cupProbe.getNewSamples()){
          Serial.println(cupProbe.getTemp());
          if(cupProbe.getMissingSamples() == 0){
            if(cupProbe.fluctuation() < 1){
              stabilityIndicatorLED.setState(LED::ON);
              if(button.getClick()){
                finalTemp = cupProbe.getTemp();
                cupProbe.routine_pause();
                stabilityIndicatorLED.setState(LED::OFF);
                //calculate calibration
                //hotWaterCapacity*(extTemp-finalTemp) <-- quantidade de calor vindo da água quente: Q = C * dT;
                internalThermalCapacity = ((hotWaterCapacity*(extTemp-finalTemp)) / (finalTemp-initTemp)) + hotWaterCapacity;
                Serial.print("Calibrado: ");
                Serial.print(internalThermalCapacity);
                Serial.println(" cal/°C");
                calibrationStep = CalibrationSteps::calibrated;
              }
            }else stabilityIndicatorLED.setState(LED::OFF);
          }
        }
        break;
        
      default: break;
    }
  }

  void measure(){
    switch(measurementStep){
      case MeasurementSteps::measureInitialTemp:
        cupProbe.clear();
        cupProbe.routine_resume();
        measurementStep = MeasurementSteps::measuringInitialTemp;
        break;
        
      case MeasurementSteps::measuringInitialTemp:
        if(cupProbe.getNewSamples()){
          Serial.println(cupProbe.getTemp());
          if(cupProbe.getMissingSamples() == 0){
            if(cupProbe.fluctuation() < 1){
              stabilityIndicatorLED.setState(LED::ON);
              if(button.getClick()){
                initTemp = cupProbe.getTemp();
                stabilityIndicatorLED.setState(LED::OFF);
                measurementStep = MeasurementSteps::measureFinalTemp;
              }
            }else stabilityIndicatorLED.setState(LED::OFF);
          }
        }
        break;

      case MeasurementSteps::measureFinalTemp:
        cupProbe.clear();
        cupProbe.routine_resume();
        measurementStep = MeasurementSteps::measuringFinalTemp;
        break;

      case MeasurementSteps::measuringFinalTemp:
        if(cupProbe.getNewSamples()){
          Serial.println(cupProbe.getTemp());
          if(cupProbe.getMissingSamples() == 0){
            if(cupProbe.fluctuation() < 1){
              stabilityIndicatorLED.setState(LED::ON);
              if(button.getClick()){
                finalTemp = cupProbe.getTemp();
                cupProbe.routine_pause();
                stabilityIndicatorLED.setState(LED::OFF);
                //calculate enthalpy
                measuredEnthalpy = internalThermalCapacity * (finalTemp-initTemp);
                Serial.print("Entalpia medida: ");
                Serial.print(measuredEnthalpy);
                Serial.println(" cal");
                measurementStep = MeasurementSteps::finished;
              }
            }else stabilityIndicatorLED.setState(LED::OFF);
          }
        }
        break;
      
      default: break;
    }
  }

  virtual void update(){
    if(calibrationStep != CalibrationSteps::calibrated){ // calibração em andamento
      calibration(); // rodar o processo de calibração
    }else{ // calibrado
      if(measurementStep != MeasurementSteps::finished){ // medição em andamento
        measure(); // rodar o processo de medição
      }else if(button.getClick()){ // ultima medição terminada, se o botão for apertado:
        measurementStep = 0; // inicio do processo de medição
      }else if(Serial.available()){
        float n = 0;
        n = Serial.parseFloat();
        if(n != 0){
          Serial.print("Entalpia de dissolução: ");
          Serial.println((-measuredEnthalpy) / n);
        }
      }
    }
  }
};


class MainRoutine : public Routine{
  Temp<> cupProbe;
  Temp<> extProbe;
  GPIO_Button button;
  LED stabilityIndicatorLED;
  Enthalpy enthalpy;

  public:
  MainRoutine(): Routine(0), cupProbe(0, 200), extProbe(1, 200), button(2, 10), stabilityIndicatorLED(4),
    enthalpy(100, cupProbe, extProbe, button, stabilityIndicatorLED) {
    extProbe.routine_pause();
  }

  protected:
  virtual void update(){
    cupProbe.routine_update();
    extProbe.routine_update();
    button.routine_update();
    enthalpy.routine_update();
  }
};

MainRoutine mainRoutine;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
}

void loop() {
  // put your main code here, to run repeatedly:
  mainRoutine.routine_update();
}
