#include "sure_smartie/serial/SerialPort.hpp"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

namespace sure_smartie::serial {

SerialPort::SerialPort(std::string device, int baudrate)
    : device_(std::move(device)), baudrate_(baudrate) {}

SerialPort::~SerialPort() { close(); }

SerialPort::SerialPort(SerialPort&& other) noexcept
    : device_(std::move(other.device_)),
      baudrate_(other.baudrate_),
      fd_(std::exchange(other.fd_, -1)) {}

SerialPort& SerialPort::operator=(SerialPort&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  close();
  device_ = std::move(other.device_);
  baudrate_ = other.baudrate_;
  fd_ = std::exchange(other.fd_, -1);
  return *this;
}

void SerialPort::open() {
  if (isOpen()) {
    return;
  }

  const int descriptor = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
  if (descriptor < 0) {
    throw std::runtime_error("Unable to open serial device " + device_ + ": " +
                             std::strerror(errno));
  }

  termios settings{};
  if (::tcgetattr(descriptor, &settings) != 0) {
    ::close(descriptor);
    throw std::runtime_error("tcgetattr failed for " + device_ + ": " +
                             std::strerror(errno));
  }

  ::cfmakeraw(&settings);
  const auto speed = static_cast<speed_t>(baudrateToConstant(baudrate_));
  ::cfsetispeed(&settings, speed);
  ::cfsetospeed(&settings, speed);

  settings.c_cflag = (settings.c_cflag & ~CSIZE) | CS8;
  settings.c_cflag |= CLOCAL | CREAD;
  settings.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
  settings.c_cflag &= ~HUPCL;
  settings.c_iflag &= ~(IXON | IXOFF | IXANY);
  settings.c_cc[VMIN] = 0;
  settings.c_cc[VTIME] = 5;

  if (::tcsetattr(descriptor, TCSANOW, &settings) != 0) {
    ::close(descriptor);
    throw std::runtime_error("tcsetattr failed for " + device_ + ": " +
                             std::strerror(errno));
  }

  fd_ = descriptor;
}

void SerialPort::close() {
  if (fd_ >= 0) {
    ::close(fd_);
    fd_ = -1;
  }
}

bool SerialPort::isOpen() const { return fd_ >= 0; }

void SerialPort::write(std::span<const std::uint8_t> data) const {
  if (!isOpen()) {
    throw std::runtime_error("Serial port is not open");
  }

  const std::uint8_t* buffer = data.data();
  std::size_t remaining = data.size();
  while (remaining > 0) {
    const auto written =
        ::write(fd_, buffer, remaining);
    if (written < 0) {
      if (errno == EINTR) {
        continue;
      }
      throw std::runtime_error("Serial write failed for " + device_ + ": " +
                               std::strerror(errno));
    }
    remaining -= static_cast<std::size_t>(written);
    buffer += written;
  }
}

int SerialPort::baudrateToConstant(int baudrate) {
  switch (baudrate) {
    case 1200:
      return B1200;
    case 2400:
      return B2400;
    case 4800:
      return B4800;
    case 9600:
      return B9600;
    case 19200:
      return B19200;
    case 38400:
      return B38400;
    case 57600:
      return B57600;
    case 115200:
      return B115200;
    default:
      throw std::invalid_argument("Unsupported baudrate: " +
                                  std::to_string(baudrate));
  }
}

}  // namespace sure_smartie::serial
