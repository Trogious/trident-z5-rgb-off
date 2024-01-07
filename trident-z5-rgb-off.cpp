// pieces extracted & trimmed from https://gitlab.com/CalcProgrammer1/OpenRGB

#include <cstring>
#include <iostream>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <sys/ioctl.h>

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef int s32;

typedef unsigned short ene_register;
typedef unsigned char ene_dev_id;

#define ENE_APPLY_VAL 0x01 /* Value for Apply Changes Register     */
#define ENE_SAVE_VAL 0xAA  /* Value for Save Changes               */

#define I2C_SMBUS_WRITE 0
#define I2C_SMBUS_BYTE_DATA 2
#define I2C_SMBUS_WORD_DATA 3

enum {
  ENE_REG_MODE = 0x8021,  /* Mode Selection Register              */
  ENE_REG_APPLY = 0x80A0, /* Apply Changes Register               */
};

#define  MAX_BYTES 4
const u8 TRIDENT_Z5_RGB_BYTES_TO_CHANGE[MAX_BYTES] = {0x70, 0x71, 0x72, 0x73};

s32 i2c_smbus_xfer(int handle, u8 addr, char read_write, u8 command, int size,
                   union i2c_smbus_data *data) {

  struct i2c_smbus_ioctl_data args;

  // Tell I2C host which slave address to transfer to
  ioctl(handle, I2C_SLAVE, addr);

  args.read_write = read_write;
  args.command = command;
  args.size = size;
  args.data = data;

  return ioctl(handle, I2C_SMBUS, &args);
}

s32 i2c_smbus_read_byte(int handle, u8 addr) {
  i2c_smbus_data data;
  if (i2c_smbus_xfer(handle, addr, I2C_SMBUS_READ, 0, I2C_SMBUS_BYTE, &data)) {
    return -1;
  } else {
    return data.byte;
  }
}

s32 i2c_smbus_read_byte_data(int handle, u8 addr, u8 command) {
    i2c_smbus_data data;
    if (i2c_smbus_xfer(handle, addr, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, &data))
    {
        return -1;
    }
    else
    {
        return data.byte;
    }
}

s32 i2c_smbus_write_byte_data(int handle, u8 addr, u8 command, u8 value) {
  i2c_smbus_data data;
  data.byte = value;
  return i2c_smbus_xfer(handle, addr, I2C_SMBUS_WRITE, command,
                        I2C_SMBUS_BYTE_DATA, &data);
}

s32 i2c_smbus_write_word_data(int handle, u8 addr, u8 command, u16 value) {
  i2c_smbus_data data;
  data.word = value;
  return i2c_smbus_xfer(handle, addr, I2C_SMBUS_WRITE, command,
                        I2C_SMBUS_WORD_DATA, &data);
}

void ENERegisterWrite(int handle, ene_dev_id dev, ene_register reg,
                      unsigned char val) {
  // Write ENE register
  i2c_smbus_write_word_data(handle, dev, 0x00,
                            ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));

  // Write ENE value
  i2c_smbus_write_byte_data(handle, dev, 0x01, val);
}

s32 ENERegisterRead(int handle, ene_dev_id dev, ene_register reg)
{
    //Write ENE register
    i2c_smbus_write_word_data(handle, dev, 0x00, ((reg << 8) & 0xFF00) | ((reg >> 8) & 0x00FF));

    //Read ENE value
    return i2c_smbus_read_byte_data(handle, dev, 0x81);
}

int main(int argc, char **argv) {
  char device_string[1024];
  DIR *dir;
  char driver_path[512];
  struct dirent *ent;
  int test_fd;
  char path[1024];
  char buff[100];
  unsigned short pci_device, pci_vendor;
  char *ptr;
  u8 new_value = 0;

  if (argc > 1) {
      new_value = atoi(argv[1]);
  }


  // Start looking for I2C adapters in /sys/bus/i2c/devices/
  strcpy(driver_path, "/sys/bus/i2c/devices/");
  dir = opendir(driver_path);

  if (dir == NULL) {
    return 0;
  }

  // Loop through all entries in i2c-adapter list
  ent = readdir(dir);

  if (ent == NULL) {
    return 0;
  }

  while (ent != NULL) {
    if (ent->d_type == DT_DIR || ent->d_type == DT_LNK) {
      if (strncmp(ent->d_name, "i2c-", 4) == 0) {
        strcpy(device_string, driver_path);
        strcat(device_string, ent->d_name);
        strcat(device_string, "/name");
        test_fd = open(device_string, O_RDONLY);

        if (test_fd) {
          memset(device_string, 0x00, sizeof(device_string));
          read(test_fd, device_string, sizeof(device_string));
          device_string[strlen(device_string) - 1] = 0x00;

          close(test_fd);

          // Clear PCI Information
          pci_vendor = 0;
          pci_device = 0;
          // Get device path
          strcpy(path, driver_path);
          strcat(path, ent->d_name);
          if (ent->d_type == DT_LNK) {
            ptr = realpath(path, NULL);
            if (ptr == NULL)
              continue;

            strcpy(path, ptr);
            strcat(path, "/..");
            free(ptr);
          } else {
            strcat(path, "/device");
          }
          ptr = path + strlen(path);
          // Get PCI Vendor
          strcpy(ptr, "/vendor");
          test_fd = open(path, O_RDONLY);
          if (test_fd >= 0) {
            memset(buff, 0x00, sizeof(buff));
            read(test_fd, buff, sizeof(buff));
            buff[strlen(buff) - 1] = 0x00;
            pci_vendor = strtoul(buff, NULL, 16);
            close(test_fd);
          }

          // Get PCI Device
          strcpy(ptr, "/device");
          test_fd = open(path, O_RDONLY);
          if (test_fd >= 0) {
            memset(buff, 0x00, sizeof(buff));
            read(test_fd, buff, sizeof(buff));
            buff[strlen(buff) - 1] = 0x00;
            pci_device = strtoul(buff, NULL, 16);
            close(test_fd);
          }

          strcpy(device_string, "/dev/");
          strcat(device_string, ent->d_name);
          test_fd = open(device_string, O_RDWR);


          if (test_fd < 0) {
            ent = readdir(dir);
          } else {
            if (pci_vendor == 0x8086 && pci_device == 0x7a23) {
              fprintf(stderr, "found device: (%#04x, %#04x) %s\n", pci_vendor, pci_device, device_string);
              for (u8 i = 0; i < MAX_BYTES; ++i) {
                  if (i2c_smbus_read_byte(test_fd, TRIDENT_Z5_RGB_BYTES_TO_CHANGE[i]) > -1) {
                    s32 previous_mode = ENERegisterRead(test_fd, TRIDENT_Z5_RGB_BYTES_TO_CHANGE[i], ENE_REG_MODE);
                    fprintf(stderr, "changing DRAM(%#02x) mode from %d to %d\n", TRIDENT_Z5_RGB_BYTES_TO_CHANGE[i], previous_mode, new_value);
                    ENERegisterWrite(test_fd, TRIDENT_Z5_RGB_BYTES_TO_CHANGE[i], ENE_REG_MODE, new_value);
                    ENERegisterWrite(test_fd, TRIDENT_Z5_RGB_BYTES_TO_CHANGE[i], ENE_REG_APPLY, ENE_APPLY_VAL);
                  } else {
                    fprintf(stderr, "cannot read byte: %#02x\n", TRIDENT_Z5_RGB_BYTES_TO_CHANGE[i]);
                  }
               }
            }
          }
        }
      }
    } else {
    }
    ent = readdir(dir);
  }
  closedir(dir);

  return 0;
}