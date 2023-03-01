/*
  xdrv_67_mcp23xxx.ino - MCP23008/MCP23017/MCP23S17 GPIO Expander support for Tasmota

  SPDX-FileCopyrightText: 2023 Theo Arends

  SPDX-License-Identifier: GPL-3.0-only
*/

#if defined(USE_I2C) || defined(USE_SPI)
#ifdef USE_MCP23XXX_DRV
/*********************************************************************************************\
 * MCP23008/17 - I2C GPIO Expander to be used as virtual button/switch/relay only
 *
 * Docs at https://www.microchip.com/wwwproducts/en/MCP23008
 *         https://www.microchip.com/wwwproducts/en/MCP23017
 *
 * I2C Address: 0x20 - 0x26 (0x27 is not supported)
 *
 * The goal of the driver is to provide a sequential list of pins configured as Tasmota template
 * and handle any input and output as configured GPIOs.
 *
 * Restrictions:
 * - Supports MCP23017 (=I2C) and MCP23S17 (=SPI)
 * - Max support for 28 switches (input), 32 buttons (input), 32 relays (output)
 *
 * Supported template fields:
 * NAME  - Template name
 * ADDR  - Sequential list of I2C addresses
 * GPIO  - Sequential list of pin 1 and up with configured GPIO function
 *         Function        Code      Description
 *         --------------  --------  ----------------------------------------
 *         None            0         Not used
 *         Button1..32     32..63    Button to Gnd with internal pullup
 *         Button_n1..32   64..95    Button to Gnd without internal pullup
 *         Button_i1..32   96..127   Button inverted to Vcc with internal pullup
 *         Button_in1..32  128..159  Button inverted to Vcc without internal pullup
 *         Switch1..28     160..187  Switch to Gnd with internal pullup
 *         Switch_n1..28   192..219  Switch to Gnd without internal pullup
 *         Relay1..28      224..255  Relay
 *         Relay_i1..28    256..287  Relay inverted
 *
 * Prepare a template to be loaded either by:
 * - a rule like: rule3 on file#mcp23x.dat do {"NAME":"MCP23017 A=Ri8-1, B=B1-8","ADDR":32,"GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]} endon
 * - a script like: -y{"NAME":"MCP23017 A=Ri8-1, B=B1-8","ADDR":32,"GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]}
 * - file called mcp23x.dat with contents: {"NAME":"MCP23017 A=Ri8-1, B=B1-8","ADDR":32,"GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]}
 *
 *                                           S3  S2  B2 B3   B1 S1    R1        R4  R2  R3  S4
 * {"NAME":"MCP23S17 Shelly Pro 4PM","GPIO":[194,193,65,66,0,64,192,0,224,0,0,0,227,225,226,195]}
 *
 * Inverted relays and buttons                          Ri8 Ri7 Ri6 Ri5 Ri4 Ri3 Ri2 Ri1 B1 B2 B3 B4 B5 B6 B7 B8
 * {"NAME":"MCP23017 A=Ri8-1, B=B1-8","ADDR":32,"GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]}
 *
 * Inverted relays and buttons                          Ri1 Ri2 Ri3 Ri4 Ri5 Ri6 Ri7 Ri8 B1 B2 B3 B4 B5 B6 B7 B8
 * {"NAME":"MCP23017 A=Ri1-8, B=B1-8","ADDR":32,"GPIO":[256,257,258,259,260,261,262,263,32,33,34,35,36,37,38,39]}
 *
 * Relays and buttons                                  R1  R2  R3  R4  R5  R6  R7  R8  B1 B2 B3 B4 B5 B6 B7 B8
 * {"NAME":"MCP23017 A=R1-8, B=B1-8","ADDR":32,"GPIO":[224,225,226,227,228,229,230,231,32,33,34,35,36,37,38,39]}
 *
 * Buttons and relays                                  B1 B2 B3 B4 B5 B6 B7 B8 R1  R2  R3  R4  R5  R6  R7  R8
 * {"NAME":"MCP23017 A=B1-8, B=R1-8","ADDR":32,"GPIO":[32,33,34,35,36,37,38,39,224,225,226,227,228,229,230,231]}
 *
 * Buttons, relays, buttons and relays                                        B1 B2 B3 B4 B5 B6 B7 B8 R1  R2  R3  R4  R5  R6  R7  R8  B9 B10B11B12B13B14B15B16R9  R10 R11 R12 R13 R14 R15 R16
 * {"NAME":"MCP23017 A=B1-8, B=R1-8, C=B9-16, D=R9-16","ADDR":[32,33],"GPIO":[32,33,34,35,36,37,38,39,224,225,226,227,228,229,230,231,40,41,42,43,44,45,46,47,232,233,234,235,236,237,238,239]}
 *
 * {"NAME":"MCP23017 A=R1-8, B=B1-8, C=R9-16, D=B9-16","ADDR":[32,33],"GPIO":[224,225,226,227,228,229,230,231,32,33,34,35,36,37,38,39,232,233,234,235,236,237,238,239,40,41,42,43,44,45,46,47]}
 *
 * 32 relays                                                                         R1  R2  R3  R4  R5  R6  R7  R8  R9  R10 R11 R12 R13 R14 R15 R16 R17 R18 R19 R20 R21 R22 R23 R24 R25 R26 R27 R28 R29 R30 R31 R32
 * {"NAME":"MCP23017 A=Ri1-8, B=Ri9-16, C=Ri17-24, D=Ri25-32","ADDR":[32,33],"GPIO":[256,257,258,259,260,261,262,263,264,265,266,267,268,269,270,271,272,273,274,275,276,277,278,279,280,281,282,283,284,285,286,287]}
 *     {"NAME":"MCP23017 A=R1-8, B=R9-16, C=R17-24, D=R25-32","ADDR":[32,33],"GPIO":[224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255]}
 *
\*********************************************************************************************/

#define XDRV_67                  67
#define XI2C_77                  77    // See I2CDEVICES.md

#define MCP23XXX_ADDR_START      0x20  // 32
#define MCP23XXX_ADDR_END        0x26  // 38

#define MCP23XXX_MAX_DEVICES     6

/*********************************************************************************************\
 * MCP23017 support
\*********************************************************************************************/

enum MCP23S08GPIORegisters {
  MCP23X08_IODIR = 0x00,
  MCP23X08_IPOL = 0x01,
  MCP23X08_GPINTEN = 0x02,
  MCP23X08_DEFVAL = 0x03,
  MCP23X08_INTCON = 0x04,
  MCP23X08_IOCON = 0x05,
  MCP23X08_GPPU = 0x06,
  MCP23X08_INTF = 0x07,
  MCP23X08_INTCAP = 0x08,
  MCP23X08_GPIO = 0x09,
  MCP23X08_OLAT = 0x0A,
};

enum MCP23X17GPIORegisters {
  // A side
  MCP23X17_IODIRA = 0x00,
  MCP23X17_IPOLA = 0x02,
  MCP23X17_GPINTENA = 0x04,
  MCP23X17_DEFVALA = 0x06,
  MCP23X17_INTCONA = 0x08,
  MCP23X17_IOCONA = 0x0A,
  MCP23X17_GPPUA = 0x0C,
  MCP23X17_INTFA = 0x0E,
  MCP23X17_INTCAPA = 0x10,
  MCP23X17_GPIOA = 0x12,
  MCP23X17_OLATA = 0x14,
  // B side
  MCP23X17_IODIRB = 0x01,
  MCP23X17_IPOLB = 0x03,
  MCP23X17_GPINTENB = 0x05,
  MCP23X17_DEFVALB = 0x07,
  MCP23X17_INTCONB = 0x09,
  MCP23X17_IOCONB = 0x0B,
  MCP23X17_GPPUB = 0x0D,
  MCP23X17_INTFB = 0x0F,
  MCP23X17_INTCAPB = 0x11,
  MCP23X17_GPIOB = 0x13,
  MCP23X17_OLATB = 0x15,
};

enum MCP23XInterruptMode { MCP23XXX_NO_INTERRUPT, MCP23XXX_CHANGE, MCP23XXX_RISING, MCP23XXX_FALLING };

enum MCP23XInterfaces { MCP23X_I2C, MCP23X_SPI };

typedef struct {
  uint8_t olata;
  uint8_t olatb;
  uint8_t address;
  uint8_t interface;
  uint8_t pins;                                        // 8 (MCP23008) or 16 (MCP23017 / MCP23S17)
  uint8_t type;                                        // 1 (MCP23008) or 2 (MCP23017) or 3 (MCP23S17)
  int8_t pin_cs;
} tMcp23xDevice;

struct MCP230 {
  tMcp23xDevice device[MCP23XXX_MAX_DEVICES];
  uint32_t relay_inverted;
  uint32_t button_inverted;
  uint8_t chip;
  uint8_t max_devices;
  uint8_t max_pins;
  uint8_t relay_max;
  uint8_t relay_offset;
  uint8_t button_max;
  uint8_t switch_max;
  int8_t button_offset;
  int8_t switch_offset;
} Mcp23x;

uint16_t *Mcp23x_gpio_pin = nullptr;

/*********************************************************************************************\
 * MCP23x17 - SPI and I2C
\*********************************************************************************************/

#ifdef USE_SPI
void MCP23xEnable(void) {
  SPI.beginTransaction(SPISettings(1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(Mcp23x.device[Mcp23x.chip].pin_cs, 0);
}

void MCP23xDisable(void) {
  SPI.endTransaction();
  digitalWrite(Mcp23x.device[Mcp23x.chip].pin_cs, 1);
}
#endif

void MCP23xDumpRegs(void) {
  uint8_t data[22];
  for (Mcp23x.chip = 0; Mcp23x.chip < Mcp23x.max_devices; Mcp23x.chip++) {
#ifdef USE_SPI
    if (MCP23X_SPI == Mcp23x.device[Mcp23x.chip].interface) {
      MCP23xEnable();
      SPI.transfer(Mcp23x.device[Mcp23x.chip].address | 1);
      SPI.transfer(0);
      for (uint32_t i = 0; i < sizeof(data); i++) {
        data[i] = SPI.transfer(0xFF);
      }
      MCP23xDisable();
    }
#endif
#ifdef USE_I2C
    if (MCP23X_I2C == Mcp23x.device[Mcp23x.chip].interface) {
      I2cReadBuffer(Mcp23x.device[Mcp23x.chip].address, 0, data, sizeof(data));
    }
#endif
    AddLog(LOG_LEVEL_DEBUG, PSTR("MCP: Intf %d, Address %02X, Regs %*_H"), Mcp23x.device[Mcp23x.chip].interface, Mcp23x.device[Mcp23x.chip].address, sizeof(data), data);
  }
}

uint32_t MCP23xRead16(uint8_t reg) {
  // Read 16-bit registers: (regb << 8) | rega
  uint32_t value = 0;
#ifdef USE_SPI
  if (MCP23X_SPI == Mcp23x.device[Mcp23x.chip].interface) {
    MCP23xEnable();
    SPI.transfer(Mcp23x.device[Mcp23x.chip].address | 1);
    SPI.transfer(reg);
    value = SPI.transfer(0xFF);  // RegA
    value |= (SPI.transfer(0xFF) << 8);   // RegB
    MCP23xDisable();
  }
#endif
#ifdef USE_I2C
  if (MCP23X_I2C == Mcp23x.device[Mcp23x.chip].interface) {
    value = I2cRead16LE(Mcp23x.device[Mcp23x.chip].address, reg);
  }
#endif
  return value;
}

uint32_t MCP23xRead(uint8_t reg) {
  uint32_t value = 0;
#ifdef USE_SPI
  if (MCP23X_SPI == Mcp23x.device[Mcp23x.chip].interface) {
    MCP23xEnable();
    SPI.transfer(Mcp23x.device[Mcp23x.chip].address | 1);
    SPI.transfer(reg);
    value = SPI.transfer(0xFF);
    MCP23xDisable();
  }
#endif
#ifdef USE_I2C
  if (MCP23X_I2C == Mcp23x.device[Mcp23x.chip].interface) {
    value = I2cRead8(Mcp23x.device[Mcp23x.chip].address, reg);
  }
#endif
  return value;
}

bool MCP23xValidRead(uint8_t reg, uint8_t *data) {
#ifdef USE_SPI
  if (MCP23X_SPI == Mcp23x.device[Mcp23x.chip].interface) {
    MCP23xEnable();
    SPI.transfer(Mcp23x.device[Mcp23x.chip].address | 1);
    SPI.transfer(reg);
    *data = SPI.transfer(0xFF);
    MCP23xDisable();
    return true;
  }
#endif
#ifdef USE_I2C
  if (MCP23X_I2C == Mcp23x.device[Mcp23x.chip].interface) {
    return I2cValidRead8(data, Mcp23x.device[Mcp23x.chip].address, reg);
  }
  return false;
#endif
}

void MCP23xWrite(uint8_t reg, uint8_t value) {
#ifdef USE_SPI
  if (MCP23X_SPI == Mcp23x.device[Mcp23x.chip].interface) {
    MCP23xEnable();
    SPI.transfer(Mcp23x.device[Mcp23x.chip].address);
    SPI.transfer(reg);
    SPI.transfer(value);
    MCP23xDisable();
  }
#endif
#ifdef USE_I2C
  if (MCP23X_I2C == Mcp23x.device[Mcp23x.chip].interface) {
    I2cWrite8(Mcp23x.device[Mcp23x.chip].address, reg, value);
  }
#endif
}

/*********************************************************************************************/

void MCP23xUpdate(uint8_t pin, bool pin_value, uint8_t reg_addr) {
  // pin = 0 - 15
  uint8_t bit = pin % 8;
  uint8_t reg_value = 0;
  if (reg_addr == MCP23X17_OLATA) {
    reg_value = Mcp23x.device[Mcp23x.chip].olata;
  } else if (reg_addr == MCP23X17_OLATB) {
    reg_value = Mcp23x.device[Mcp23x.chip].olatb;
  } else {
    reg_value = MCP23xRead(reg_addr);
  }
  if (pin_value) {
    reg_value |= 1 << bit;
  } else {
    reg_value &= ~(1 << bit);
  }
  MCP23xWrite(reg_addr, reg_value);
  if (reg_addr == MCP23X17_OLATA) {
    Mcp23x.device[Mcp23x.chip].olata = reg_value;
  } else if (reg_addr == MCP23X17_OLATB) {
    Mcp23x.device[Mcp23x.chip].olatb = reg_value;
  }
}

/*********************************************************************************************/

void MCP23xPinMode(uint8_t pin, uint8_t flags) {
  // pin 0 - 31
  Mcp23x.chip = pin / 16;
  pin = pin % 16;
  uint8_t iodir = pin < 8 ? MCP23X17_IODIRA : MCP23X17_IODIRB;
  uint8_t gppu = pin < 8 ? MCP23X17_GPPUA : MCP23X17_GPPUB;
  switch (flags) {
    case INPUT:
      MCP23xUpdate(pin, true, iodir);
      MCP23xUpdate(pin, false, gppu);
      break;
    case INPUT_PULLUP:
      MCP23xUpdate(pin, true, iodir);
      MCP23xUpdate(pin, true, gppu);
      break;
    case OUTPUT:
      MCP23xUpdate(pin, false, iodir);
      break;
  }
}

void MCP23xPinInterruptMode(uint8_t pin, uint8_t interrupt_mode) {
  // pin 0 - 31
  Mcp23x.chip = pin / 16;
  pin = pin % 16;
  uint8_t gpinten = pin < 8 ? MCP23X17_GPINTENA : MCP23X17_GPINTENB;
  uint8_t intcon = pin < 8 ? MCP23X17_INTCONA : MCP23X17_INTCONB;
  uint8_t defval = pin < 8 ? MCP23X17_DEFVALA : MCP23X17_DEFVALB;
  switch (interrupt_mode) {
    case MCP23XXX_CHANGE:
      MCP23xUpdate(pin, true, gpinten);
      MCP23xUpdate(pin, false, intcon);
      break;
    case MCP23XXX_RISING:
      MCP23xUpdate(pin, true, gpinten);
      MCP23xUpdate(pin, true, intcon);
      MCP23xUpdate(pin, true, defval);
      break;
    case MCP23XXX_FALLING:
      MCP23xUpdate(pin, true, gpinten);
      MCP23xUpdate(pin, true, intcon);
      MCP23xUpdate(pin, false, defval);
      break;
    case MCP23XXX_NO_INTERRUPT:
      MCP23xUpdate(pin, false, gpinten);
      break;
  }
}

bool MCP23xDigitalRead(uint8_t pin) {
  // pin 0 - 31
  Mcp23x.chip = pin / 16;
  pin = pin % 16;
  uint8_t bit = pin % 8;
  uint8_t reg_addr = pin < 8 ? MCP23X17_GPIOA : MCP23X17_GPIOB;
  uint8_t value = MCP23xRead(reg_addr);
  return value & (1 << bit);
}

void MCP23xDigitalWrite(uint8_t pin, bool value) {
  // pin 0 - 31
  Mcp23x.chip = pin / 16;
  pin = pin % 16;
  uint8_t reg_addr = pin < 8 ? MCP23X17_OLATA : MCP23X17_OLATB;
  MCP23xUpdate(pin, value, reg_addr);
}

/*********************************************************************************************\
 * Tasmota
\*********************************************************************************************/

int IRAM_ATTR MCP23xPin(uint32_t gpio, uint32_t index = 0);
int IRAM_ATTR MCP23xPin(uint32_t gpio, uint32_t index) {
  uint16_t real_gpio = gpio << 5;
  uint16_t mask = 0xFFE0;
  if (index < GPIO_ANY) {
    real_gpio += index;
    mask = 0xFFFF;
  }
  for (uint32_t i = 0; i < Mcp23x.max_pins; i++) {
    if ((Mcp23x_gpio_pin[i] & mask) == real_gpio) {
      return i;                                        // Pin number configured for gpio
    }
  }
  return -1;                                           // No pin used for gpio
}

bool MCP23xPinUsed(uint32_t gpio, uint32_t index = 0);
bool MCP23xPinUsed(uint32_t gpio, uint32_t index) {
  return (MCP23xPin(gpio, index) >= 0);
}

uint32_t MCP23xGetPin(uint32_t lpin) {
  if (lpin < Mcp23x.max_pins) {
    return Mcp23x_gpio_pin[lpin];
  } else {
    return GPIO_NONE;
  }
}

/*********************************************************************************************/

String MCP23xTemplateLoadFile(void) {
  String mcptmplt = "";
#ifdef USE_UFILESYS
  mcptmplt = TfsLoadString("/mcp23x.dat");
#endif  // USE_UFILESYS
#ifdef USE_RULES
  if (!mcptmplt.length()) {
    mcptmplt = RuleLoadFile("MCP23X.DAT");
  }
#endif  // USE_RULES
#ifdef USE_SCRIPT
  if (!mcptmplt.length()) {
    mcptmplt = ScriptLoadSection(">y");
  }
#endif  // USE_SCRIPT
  return mcptmplt;
}

bool MCP23xLoadTemplate(void) {
  String mcptmplt = MCP23xTemplateLoadFile();
  uint32_t len = mcptmplt.length() +1;
  if (len < 7) { return false; }                       // No McpTmplt found

  JsonParser parser((char*)mcptmplt.c_str());
  JsonParserObject root = parser.getRootObject();
  if (!root) { return false; }

  // rule3 on file#mcp23x.dat do {"NAME":"MCP23017","ADDR":32,"GPIO":[32,33,34,35,36,37,38,39,224,225,226,227,228,229,230,231]} endon
  // rule3 on file#mcp23x.dat do {"NAME":"MCP23017","ADDR":32,"GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]} endon
  // rule3 on file#mcp23x.dat do {"NAME":"MCP23017 A=Ri8-1, B=B1-8","ADDR":[32,33],"GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39]} endon
  // rule3 on file#mcp23x.dat do {"NAME":"MCP23017 A=Ri8-1, B=B1-8, C=Ri16-9, D=B9-16","ADDR":[32,33],"GPIO":[263,262,261,260,259,258,257,256,32,33,34,35,36,37,38,39,271,270,269,268,267,266,265,264,40,41,42,43,44,45,46,47]} endon

  // {"NAME":"MCP23017","ADDR":32,"GPIO":[32,33,34,35,36,37,38,39,224,225,226,227,228,229,230,231]}
  // {"NAME":"MCP23017","ADDR":[32,33],"GPIO":[32,33,34,35,36,37,38,39,224,225,226,227,228,229,230,231,40,41,42,43,44,45,46,47,232,233,234,235,236,237,238,239]}
  JsonParserToken val = root[PSTR(D_JSON_NAME)];
  if (val) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("MCP: Template %s"), val.getStr());
  }
/*
  JsonParserArray arr = root[PSTR("ADDR")];
  if (arr) {
    uint32_t i;
    for (i = 0; i < Mcp23x.max_devices; i++) {
      JsonParserToken val = arr[i];
      if (!val) { break; }

      uint8_t address = val.getUInt();
      uint32_t j;
      for (j = 0; j < Mcp23x.max_devices; j++) {
        if (address == Mcp23x.device[Mcp23x.max_devices].address) {
          j = 0;
          break;
        }
      }
      if (j) { return false; }                         // Requested I2C address not present

    }
  } else {
    val = root[PSTR("ADDR")];

  }
*/
  JsonParserArray arr = root[PSTR(D_JSON_GPIO)];
  if (arr) {
    uint32_t pin = 0;
    for (pin; pin < Mcp23x.max_pins; pin++) {          // Max number of detected chip pins
      JsonParserToken val = arr[pin];
      if (!val) { break; }
      uint16_t mpin = val.getUInt();
      if (mpin) {                                      // Above GPIO_NONE
        if ((mpin >= AGPIO(GPIO_SWT1)) && (mpin < (AGPIO(GPIO_SWT1) + MAX_SWITCHES_SET))) {
          Mcp23x.switch_max++;
          MCP23xPinMode(pin, INPUT_PULLUP);
//          MCP23xPinInterruptMode(pin, MCP23XXX_CHANGE);
        }
        else if ((mpin >= AGPIO(GPIO_SWT1_NP)) && (mpin < (AGPIO(GPIO_SWT1_NP) + MAX_SWITCHES_SET))) {
          mpin -= (AGPIO(GPIO_SWT1_NP) - AGPIO(GPIO_SWT1));
          Mcp23x.switch_max++;
          MCP23xPinMode(pin, INPUT);
//          MCP23xPinInterruptMode(pin, MCP23XXX_CHANGE);
        }
        else if ((mpin >= AGPIO(GPIO_KEY1)) && (mpin < (AGPIO(GPIO_KEY1) + MAX_KEYS_SET))) {
          Mcp23x.button_max++;
          MCP23xPinMode(pin, INPUT_PULLUP);
//          MCP23xPinInterruptMode(pin, MCP23XXX_CHANGE);
        }
        else if ((mpin >= AGPIO(GPIO_KEY1_NP)) && (mpin < (AGPIO(GPIO_KEY1_NP) + MAX_KEYS_SET))) {
          mpin -= (AGPIO(GPIO_KEY1_NP) - AGPIO(GPIO_KEY1));
          Mcp23x.button_max++;
          MCP23xPinMode(pin, INPUT);
//          MCP23xPinInterruptMode(pin, MCP23XXX_CHANGE);
        }
        else if ((mpin >= AGPIO(GPIO_KEY1_INV)) && (mpin < (AGPIO(GPIO_KEY1_INV) + MAX_KEYS_SET))) {
          bitSet(Mcp23x.button_inverted, mpin - AGPIO(GPIO_KEY1_INV));
          mpin -= (AGPIO(GPIO_KEY1_INV) - AGPIO(GPIO_KEY1));
          Mcp23x.button_max++;
          MCP23xPinMode(pin, INPUT_PULLUP);
//          MCP23xPinInterruptMode(pin, MCP23XXX_CHANGE);
        }
        else if ((mpin >= AGPIO(GPIO_KEY1_INV_NP)) && (mpin < (AGPIO(GPIO_KEY1_INV_NP) + MAX_KEYS_SET))) {
          bitSet(Mcp23x.button_inverted, mpin - AGPIO(GPIO_KEY1_INV_NP));
          mpin -= (AGPIO(GPIO_KEY1_INV_NP) - AGPIO(GPIO_KEY1));
          Mcp23x.button_max++;
          MCP23xPinMode(pin, INPUT);
//          MCP23xPinInterruptMode(pin, MCP23XXX_CHANGE);
        }
        else if ((mpin >= AGPIO(GPIO_REL1)) && (mpin < (AGPIO(GPIO_REL1) + MAX_RELAYS_SET))) {
          Mcp23x.relay_max++;
          MCP23xPinMode(pin, OUTPUT);
        }
        else if ((mpin >= AGPIO(GPIO_REL1_INV)) && (mpin < (AGPIO(GPIO_REL1_INV) + MAX_RELAYS_SET))) {
          bitSet(Mcp23x.relay_inverted, mpin - AGPIO(GPIO_REL1_INV));
          mpin -= (AGPIO(GPIO_REL1_INV) - AGPIO(GPIO_REL1));
          Mcp23x.relay_max++;
          MCP23xPinMode(pin, OUTPUT);
        }
        else { mpin = 0; }
        Mcp23x_gpio_pin[pin] = mpin;
      }
      if ((Mcp23x.switch_max >= MAX_SWITCHES_SET) ||
          (Mcp23x.button_max >= MAX_KEYS_SET) ||
          (Mcp23x.relay_max >= MAX_RELAYS_SET)) {
        AddLog(LOG_LEVEL_INFO, PSTR("MCP: Max reached (S%d/B%d/R%d)"), Mcp23x.switch_max, Mcp23x.button_max, Mcp23x.relay_max);
        break;
      }
    }
    Mcp23x.max_pins = pin;                             // Max number of configured pins
  }

//  AddLog(LOG_LEVEL_DEBUG, PSTR("MCP: Pins %d, Mcp23x_gpio_pin %*_V"), Mcp23x.max_pins, Mcp23x.max_pins, (uint8_t*)Mcp23x_gpio_pin);

  return true;
}

uint32_t MCP23xTemplateGpio(void) {
  String mcptmplt = MCP23xTemplateLoadFile();
  uint32_t len = mcptmplt.length() +1;
  if (len < 7) { return 0; }                           // No McpTmplt found

  JsonParser parser((char*)mcptmplt.c_str());
  JsonParserObject root = parser.getRootObject();
  if (!root) { return 0; }

  JsonParserArray arr = root[PSTR(D_JSON_GPIO)];
  if (arr.isArray()) {
    return arr.size();                                // Number of requested pins
  }
  return 0;
}

void MCP23xModuleInit(void) {
  int32_t pins_needed = MCP23xTemplateGpio();
  if (!pins_needed) {
    AddLog(LOG_LEVEL_DEBUG, PSTR("MCP: Invalid template"));
    return;
  }

#ifdef USE_SPI
  if ((SPI_MOSI_MISO == TasmotaGlobal.spi_enabled) && PinUsed(GPIO_MCP23SXX_CS, GPIO_ANY)) {
    SPI.begin(Pin(GPIO_SPI_CLK), Pin(GPIO_SPI_MISO), Pin(GPIO_SPI_MOSI), -1);
    while ((Mcp23x.max_devices < MCP23XXX_MAX_DEVICES) && PinUsed(GPIO_MCP23SXX_CS, Mcp23x.max_devices)) {
      Mcp23x.device[Mcp23x.chip].pin_cs = Pin(GPIO_MCP23SXX_CS, Mcp23x.max_devices);
      digitalWrite(Mcp23x.device[Mcp23x.chip].pin_cs, 1);
      pinMode(Mcp23x.device[Mcp23x.chip].pin_cs, OUTPUT);
      Mcp23x.device[Mcp23x.chip].interface = MCP23X_SPI;
      Mcp23x.device[Mcp23x.chip].address = MCP23XXX_ADDR_START << 1;
      AddLog(LOG_LEVEL_INFO, PSTR("SPI: MCP23S17 found"));
      Mcp23x.device[Mcp23x.chip].type = 3;
      Mcp23x.device[Mcp23x.chip].pins = 16;
      MCP23xWrite(MCP23X17_IOCONA, 0b01011000);    // Enable INT mirror, Slew rate disabled, HAEN pins for addressing
      Mcp23x.device[Mcp23x.chip].olata = MCP23xRead(MCP23X17_OLATA);
      Mcp23x.device[Mcp23x.chip].olatb = MCP23xRead(MCP23X17_OLATB);
      Mcp23x.max_devices++;

      Mcp23x.max_pins += Mcp23x.device[Mcp23x.chip].pins;
      pins_needed -= Mcp23x.device[Mcp23x.chip].pins;
      if (!pins_needed) { break; }
    }
  } else {
#endif  // USE_SPI
    uint8_t mcp23xxx_address = MCP23XXX_ADDR_START;
    while ((Mcp23x.max_devices < MCP23XXX_MAX_DEVICES) && (mcp23xxx_address < MCP23XXX_ADDR_END)) {
      Mcp23x.chip = Mcp23x.max_devices;
      if (I2cSetDevice(mcp23xxx_address)) {
        Mcp23x.device[Mcp23x.chip].interface = MCP23X_I2C;
        Mcp23x.device[Mcp23x.chip].address = mcp23xxx_address;

        MCP23xWrite(MCP23X08_IOCON, 0x80);               // Attempt to set bank mode - this will only work on MCP23017, so its the best way to detect the different chips 23008 vs 23017
        uint8_t buffer;
        if (MCP23xValidRead(MCP23X08_IOCON, &buffer)) {
          if (0x00 == buffer) {
  /*
            I2cSetActiveFound(mcp23xxx_address, "MCP23008");
            Mcp23x.device[Mcp23x.chip].type = 1;
            Mcp23x.device[Mcp23x.chip].pins = 8;
            Mcp23x.max_devices++;
  */
          }
          else if (0x80 == buffer) {
            I2cSetActiveFound(mcp23xxx_address, "MCP23017");
            Mcp23x.device[Mcp23x.chip].type = 2;
            Mcp23x.device[Mcp23x.chip].pins = 16;
            MCP23xWrite(MCP23X08_IOCON, 0x00);           // Reset bank mode to 0 (MCP23X17_GPINTENB)
            MCP23xWrite(MCP23X17_IOCONA, 0b01011000);    // Enable INT mirror, Slew rate disabled, HAEN pins for addressing
            Mcp23x.device[Mcp23x.chip].olata = MCP23xRead(MCP23X17_OLATA);
            Mcp23x.device[Mcp23x.chip].olatb = MCP23xRead(MCP23X17_OLATB);
            Mcp23x.max_devices++;
          }
          Mcp23x.max_pins += Mcp23x.device[Mcp23x.chip].pins;
          pins_needed -= Mcp23x.device[Mcp23x.chip].pins;
        }
      }
      if (pins_needed) {
        mcp23xxx_address++;
      } else {
        mcp23xxx_address = MCP23XXX_ADDR_END;
      }
    }
#ifdef USE_SPI
  }
#endif  // USE_SPI

  if (!Mcp23x.max_devices) { return; }

  Mcp23x_gpio_pin = (uint16_t*)calloc(Mcp23x.max_pins, 2);
  if (!Mcp23x_gpio_pin) { return; }

  if (!MCP23xLoadTemplate()) {
    AddLog(LOG_LEVEL_INFO, PSTR("MCP: No valid template found"));  // Too many GPIO's
    Mcp23x.max_devices = 0;
    return;
  }

  Mcp23x.relay_offset = TasmotaGlobal.devices_present;
  Mcp23x.relay_max -= UpdateDevicesPresent(Mcp23x.relay_max);

  Mcp23x.button_offset = -1;
  Mcp23x.switch_offset = -1;
}

void MCP23xServiceInput(void) {
  // I found no way to receive interrupts in a consistent manner; noise received at undefined moments - unstable usage
  // This works with no interrupt
  uint32_t pin_offset = 0;
  for (Mcp23x.chip = 0; Mcp23x.chip < Mcp23x.max_devices; Mcp23x.chip++) {
    uint32_t gpio = MCP23xRead16(MCP23X17_GPIOA);      // Read gpio

//    AddLog(LOG_LEVEL_DEBUG_MORE, PSTR("MCP: Chip %d, State %04X"), Mcp23x.chip, gpio);

    uint32_t mask = 1;
    for (uint32_t pin = 0; pin < Mcp23x.device[Mcp23x.chip].pins; pin++) {
      uint32_t state = ((gpio & mask) != 0);
      uint32_t lpin = MCP23xGetPin(pin_offset + pin);  // 0 for None, 32 for KEY1, 160 for SWT1, 224 for REL1
      uint32_t index = lpin & 0x001F;                  // Max 32 buttons or switches
      lpin = BGPIO(lpin);                              // UserSelectablePins number
      if (GPIO_KEY1 == lpin) {
        ButtonSetVirtualPinState(Mcp23x.button_offset + index, (state != bitRead(Mcp23x.button_inverted, index)));
      }
      else if (GPIO_SWT1 == lpin) {
        SwitchSetVirtualPinState(Mcp23x.switch_offset + index, state);
      }
      mask <<= 1;
    }
    pin_offset += Mcp23x.device[Mcp23x.chip].pins;
  }
}

void MCP23xPower(void) {
  power_t rpower = XdrvMailbox.index >> Mcp23x.relay_offset;
  for (uint32_t index = 0; index < Mcp23x.relay_max; index++) {
    power_t state = rpower &1;
    if (MCP23xPinUsed(GPIO_REL1, index)) {
      uint32_t pin = MCP23xPin(GPIO_REL1, index) & 0x3F;   // Fix possible overflow over 63 gpios

//      AddLog(LOG_LEVEL_DEBUG, PSTR("MCP: Power pin %d, state %d(%d)"), pin, state, bitRead(Mcp23x.relay_inverted, index));

      MCP23xDigitalWrite(pin, bitRead(Mcp23x.relay_inverted, index) ? !state : state);
    }
    rpower >>= 1;                                      // Select next power
  }
}

bool MCP23xAddButton(void) {
  // XdrvMailbox.index = button/switch index
  if (Mcp23x.button_offset < 0) { Mcp23x.button_offset = XdrvMailbox.index; }
  uint32_t index = XdrvMailbox.index - Mcp23x.button_offset;
  if (index >= Mcp23x.button_max) { return false; }
  XdrvMailbox.index = (MCP23xDigitalRead(MCP23xPin(GPIO_KEY1, index)) != bitRead(Mcp23x.button_inverted, index));

//  AddLog(LOG_LEVEL_DEBUG, PSTR("MCP: AddButton index %d, state %d"), index, XdrvMailbox.index);

  return true;
}

bool MCP23xAddSwitch(void) {
  // XdrvMailbox.index = button/switch index
  if (Mcp23x.switch_offset < 0) { Mcp23x.switch_offset = XdrvMailbox.index; }
  uint32_t index = XdrvMailbox.index - Mcp23x.switch_offset;
  if (index >= Mcp23x.switch_max) { return false; }
  XdrvMailbox.index = MCP23xDigitalRead(MCP23xPin(GPIO_SWT1, index));
  return true;
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv67(uint32_t function) {
  bool spi_enabled = false;
  bool i2c_enabled = false;
#ifdef USE_SPI
  spi_enabled = (SPI_MOSI_MISO == TasmotaGlobal.spi_enabled);
#endif  // USE_SPI
#ifdef USE_I2C
  i2c_enabled = I2cEnabled(XI2C_77);
#endif  // USE_I2C
  if (!spi_enabled && !i2c_enabled) { return false; }

  bool result = false;

  if (FUNC_MODULE_INIT == function) {
    MCP23xModuleInit();
  } else if (Mcp23x.max_devices) {
    switch (function) {
      case FUNC_EVERY_100_MSECOND:
        if (Mcp23x.button_max || Mcp23x.switch_max) {
          MCP23xServiceInput();
        }
        break;
      case FUNC_SET_POWER:
        MCP23xPower();
        break;
      case FUNC_ADD_BUTTON:
        result = MCP23xAddButton();
        break;
      case FUNC_ADD_SWITCH:
        result = MCP23xAddSwitch();
        break;
    }
  }
  return result;
}

#endif  // USE_MCP23XXX_DRV
#endif  // USE_I2C or USE_ESP_SPI
