#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace sure_smartie::serial {

class SerialPort {
 public:
  SerialPort(std::string device, int baudrate);
  ~SerialPort();

  SerialPort(const SerialPort&) = delete;
  SerialPort& operator=(const SerialPort&) = delete;

  SerialPort(SerialPort&& other) noexcept;
  SerialPort& operator=(SerialPort&& other) noexcept;

  void open();
  void close();
  [[nodiscard]] bool isOpen() const;
  void write(std::span<const std::uint8_t> data) const;

 private:
  static int baudrateToConstant(int baudrate);

  std::string device_;
  int baudrate_;
  int fd_{-1};
};

}  // namespace sure_smartie::serial
