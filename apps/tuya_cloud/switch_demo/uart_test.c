#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <time.h>

int main()
{
    int fd;
    struct termios tty;
    char buffer[256];
    int n;

    // 打开串口设备
    fd = open("/dev/ttyUSB0", O_RDWR | O_NOCTTY);
    if (fd < 0) {
        printf("Error opening /dev/ttyUSB0: %s\n", strerror(errno));
        return -1;
    }

    // 配置串口
    if (tcgetattr(fd, &tty) < 0) {
        printf("Error getting serial attributes: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    // 设置波特率
    cfsetospeed(&tty, B921600);
    cfsetispeed(&tty, B921600);

    // 8N1
    tty.c_cflag &= ~PARENB; // 无奇偶校验
    tty.c_cflag &= ~CSTOPB; // 1个停止位
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8; // 8个数据位

    // 禁用硬件流控制
    tty.c_cflag &= ~CRTSCTS;

    // 启用接收器，忽略调制解调器控制线
    tty.c_cflag |= CREAD | CLOCAL;

    // 禁用canonical模式，禁用echo，禁用erasure，禁用new-line echo
    tty.c_lflag &= ~ICANON;
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;

    // 禁用软件流控制
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);

    // 禁用特殊字符处理
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);

    // 防止输出处理
    tty.c_oflag &= ~OPOST;
    tty.c_oflag &= ~ONLCR;

    // 设置超时
    tty.c_cc[VTIME] = 10; // 1秒超时
    tty.c_cc[VMIN] = 0;   // 非阻塞读取

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        printf("Error setting serial attributes: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    printf("Serial port opened successfully, sending AT command...\n");

    // 清空输入缓冲区
    tcflush(fd, TCIOFLUSH);
    usleep(100000); // 等待100ms

    // 发送AT命令
    const char *at_cmd = "AT\r\n";
    if (write(fd, at_cmd, strlen(at_cmd)) < 0) {
        printf("Error writing to serial port: %s\n", strerror(errno));
        close(fd);
        return -1;
    }

    printf("AT command sent, waiting for response...\n");

    // 等待并读取响应
    for (int i = 0; i < 30; i++) { // 最多等待3秒
        n = read(fd, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            printf("Received: %s", buffer);
            // 检查是否收到完整响应
            if (strstr(buffer, "OK") || strstr(buffer, "ERROR")) {
                break;
            }
        } else if (n < 0 && errno != EAGAIN) {
            printf("Error reading from serial port: %s\n", strerror(errno));
            break;
        }
        usleep(100000); // 等待100ms
    }

    close(fd);
    printf("Test completed.\n");
    return 0;
}
