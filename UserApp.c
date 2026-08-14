#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define DEVICE "/dev/stm32_usb0"

/* Commands */
#define CMD_LED_ON        0x01
#define CMD_LED_OFF       0x02
#define CMD_READ_SENSOR   0x10   /*NEW: RFID + Sensor */

/* Response Status Codes */
#define RESP_AUTHORIZED   0xA0   /* RFID authorized */
#define RESP_DENIED       0xA1   /* RFID denied */
#define RESP_TIMEOUT      0xA2   /* No card detected */

#define ACK_SUCCESS       0xAA

/* Response Packet Structure */
typedef struct __attribute__((packed))
{
    uint8_t status;
    float   temperature;
    float   pressure;
    uint8_t uid[4];
} Sensor_Response_t;

int main()
{
    int fd;
    int choice;
    uint8_t cmd;
    uint8_t ack;
    Sensor_Response_t response;

    fd = open(DEVICE, O_RDWR);

    if(fd < 0)
    {
        perror("open");
        return -1;
    }

    while(1)
    {
        printf("\n");
        printf("═══════════════════════════════════════\n");
        printf(" STM32 USB Device Control\n");
        printf("═══════════════════════════════════════\n");
        printf("1. LED ON\n");
        printf("2. LED OFF\n");
        printf("3. Read Sensor Data (RFID Auth)\n");
        printf("4. Exit\n");
        printf("═══════════════════════════════════════\n");
        printf("Enter Choice : ");
        scanf("%d",&choice);

        switch(choice)
        {
            case 1:
                cmd = CMD_LED_ON;
                write(fd, &cmd, 1);
                read(fd, &ack, 1);

                if(ack == ACK_SUCCESS)
                    printf(" LED Turned ON Successfully\n");
                else
                    printf(" Command Failed\n");
                break;

            case 2:
                cmd = CMD_LED_OFF;
                write(fd, &cmd, 1);
                read(fd, &ack, 1);

                if(ack == ACK_SUCCESS)
                    printf(" LED Turned OFF Successfully\n");
                else
                    printf(" Command Failed\n");
                break;

            case 3:
                printf("\n");
                printf("╔══════════════════════════════════════╗\n");
                printf("║  Please tap RFID card on STM32       ║\n");
                printf("║                                      ║\n");
                printf("║  Blue  = Waiting for card            ║\n");
                printf("║  Green = Authorized                  ║\n");
                printf("║  Red   = Denied/Timeout              ║\n");
                printf("║                                      ║\n");
                printf("║  Timeout: 10 seconds                 ║\n");
                printf("╚══════════════════════════════════════╝\n");
                printf("\nWaiting...\n\n");

                cmd = CMD_READ_SENSOR;
                write(fd, &cmd, 1);

                int bytes = read(fd, &response, sizeof(Sensor_Response_t));

                if(bytes == sizeof(Sensor_Response_t))
                {
                    printf("\n");
                    
                    if(response.status == RESP_AUTHORIZED)
                    {
                        /* AUTHORIZED */
                        printf("╔══════════════════════════════════════╗\n");
                        printf("║        ACCESS GRANTED                ║\n");
                        printf("╠══════════════════════════════════════╣\n");
                        printf("║  Card UID: %02X %02X %02X %02X               ║\n",
                               response.uid[0],
                               response.uid[1],
                               response.uid[2],
                               response.uid[3]);
                        printf("╠══════════════════════════════════════╣\n");
                        printf("║    Temperature : %.2f °C            ║\n",
                               response.temperature);
                        printf("║    Pressure    : %.2f hPa          ║\n",
                               response.pressure);
                        printf("╚══════════════════════════════════════╝\n");
                    }
                    else if(response.status == RESP_DENIED)
                    {
                        /* NOT AUTHORIZED */
                        printf("╔══════════════════════════════════════╗\n");
                        printf("║        ACCESS DENIED                 ║\n");
                        printf("╠══════════════════════════════════════╣\n");
                        printf("║  Card UID: %02X %02X %02X %02X               ║\n",
                               response.uid[0],
                               response.uid[1],
                               response.uid[2],
                               response.uid[3]);
                        printf("╠══════════════════════════════════════╣\n");
                        printf("║  This card is not authorized!        ║\n");
                        printf("╚══════════════════════════════════════╝\n");
                    }
                    else if(response.status == RESP_TIMEOUT)
                    {
                        /* TIMEOUT */
                        printf("╔══════════════════════════════════════╗\n");
                        printf("║        TIMEOUT                       ║\n");
                        printf("╠══════════════════════════════════════╣\n");
                        printf("║  No card detected in 10 seconds      ║\n");
                        printf("╚══════════════════════════════════════╝\n");
                    }
                    else
                    {
                        /* UNKNOWN STATUS */
                        printf(" Unknown response status: 0x%02X\n", response.status);
                    }
                }
                else
                {
                    printf(" Communication Error (received %d bytes)\n", bytes);
                }
                break;

            case 4:
                close(fd);
                return 0;

            default:
                printf(" Invalid Choice\n");
        }
    }
}
