
#define _XOPEN_SOURCE 700
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <assert.h>
#include <time.h>
#include <stdbool.h>

#define MAX_TOKEN 20
#define MAX_TOKEN_LENGTH 50
#define MAX_USER_INPUT 200

#define TENANT_NUM 5
#define DEVICE_NUM 12
#define ROOM_NUM 3
#define BOOKING_NUM 600
#define BOOKING_TYPE 4

#define FIRST_DAY "2021-05-10"

#define DEVICE_NAME_LEN 15
#define ROOM_NAME_LEN 7
#define TENANT_NAME_LEN 9
#define DATE_FORMAT_LEN 11
#define TIME_FORMAT_LEN 6
#define TYPE_NAME_LEN 13
#define REPORT_PREFIX "RBM_Report_G14_"
#define SCHEDULE_PREFIX "RBM_Schedule_G14_"

#define MESSAGE_SIZE 9
#define RESPONSE_SIZE 3
#define MAX_BOOKING_DAY 7
#define TIMESLOT_PER_DAY 24

#define MAX_MESSAGE_TOKEN_NUM 3
#define MAX_MESSAGE_TOKEN_LEN 9

//to store a token(string)
typedef struct token
{
    char *token;
} token;

//to store the user input command
typedef struct cmd
{
    //string of storing the whole command
    char *userInput;
    //to store the token(string) of tokenized command (spilited command)
    token *tokens;
    //number of token(string)
    int tokenNum;
} cmd;

//to store a booking
typedef struct booking
{
    int type;
    struct tm *start_time;
    float duration;
    int peopleNum;
    char *deviceA;
    char *deviceB;
    char *caller;
    int roomNum;
    int accepted;
    int deviceA_index;
    int deviceB_index;
} booking;

//to store all bookings and the system
typedef struct rbm
{
    booking *bookings;
    int bookingSize;
    int reportNum;
    int invalid_req;
} rbm;

const char *DEVICE_NAME[] = {
    "webcam_FHD_1",
    "webcam_FHD_2",
    "webcam_UHD",
    "monitor_50_1",
    "monitor_50_2",
    "monitor_75",
    "projector_2K_1",
    "projector_2K_2",
    "projector_4K",
    "screen_100_1",
    "screen_100_2",
    "screen_150",
};

const char *TENANT_NAME[] = {
    "tenant_A",
    "tenant_B",
    "tenant_C",
    "tenant_D",
    "tenant_E",
};

const char *ROOM_NAME[] = {
    "room_A",
    "room_B",
    "room_C",
};

const int ROOM_CAPACITY[ROOM_NUM] = {10, 10, 20};

void ignore_first_char(char *dest, char *src, int size);
void read_batch_file(rbm *rbms, char *file_name);

// constructor part

// allocate and initialize memory to struct token
void token_constructor(token *tok)
{
    tok->token = (char *)calloc(MAX_TOKEN_LENGTH, sizeof(char));
    memset(tok->token, '\0', sizeof(tok->token));
}

// allocate and initialize memory to struct cmd
void cmd_constructor(cmd *cmds)
{
    int i;
    cmds->userInput = (char *)calloc(MAX_USER_INPUT, sizeof(char));
    cmds->tokens = (token *)calloc(MAX_TOKEN, sizeof(token));
    for (i = 0; i < MAX_TOKEN; i++)
    {
        token_constructor(cmds->tokens + i);
    }
    memset(cmds->userInput, '\0', sizeof(cmds->userInput));
}

// allocate and initialize memory to struct booking
void booking_constructor(booking *booking)
{
    booking->type = 0;
    booking->start_time = (struct tm *)calloc(1, sizeof(struct tm));
    if (booking->start_time == NULL)
    {
        printf("Failure to allocate room for the array\n");
        exit(0);
    }
    booking->duration = 0;
    booking->peopleNum = 0;
    booking->deviceA = (char *)calloc(DEVICE_NAME_LEN, sizeof(char));
    memset(booking->deviceA, '\0', DEVICE_NAME_LEN);
    if (booking->deviceA == NULL)
    {
        printf("Failure to allocate room for the array\n");
        exit(0);
    }
    booking->deviceB = (char *)calloc(DEVICE_NAME_LEN, sizeof(char));
    memset(booking->deviceB, '\0', DEVICE_NAME_LEN);
    if (booking->deviceB == NULL)
    {
        printf("Failure to allocate room for the array\n");
        exit(0);
    }
    booking->caller = (char *)calloc(TENANT_NAME_LEN, sizeof(char));
    if (booking->caller == NULL)
    {
        printf("Failure to allocate room for the array\n");
        exit(0);
    }
    booking->accepted = 0;
    booking->roomNum = -1;
    booking->deviceA_index = -1;
    booking->deviceB_index = -1;
    memset(booking->deviceA, '\0', sizeof(DEVICE_NAME_LEN));
    memset(booking->deviceB, '\0', sizeof(DEVICE_NAME_LEN));
    memset(booking->caller, '\0', sizeof(TENANT_NAME_LEN));
}

// allocate and initialize memory to struct rbm
void rbm_constructor(rbm *rbm)
{
    int i;
    rbm->reportNum = 0;
    rbm->invalid_req = 0;
    rbm->bookingSize = 0;
    rbm->bookings = (booking *)calloc(BOOKING_NUM, sizeof(booking));
    for (i = 0; i < BOOKING_NUM; i++)
    {
        booking_constructor(rbm->bookings + i);
    }
}

// return the index of DEVICE_NAME that matches the string in the first parameter char *name
int get_device_index(char *name)
{
    int i;
    for (i = 0; i < DEVICE_NUM; i++)
    {
        if (strstr(DEVICE_NAME[i], name) != NULL)
        {
            return i;
        }
    }
    return -1;
}

// the first parameter device_type is the integer representing the type of device.
// return the number of device available from the each type of device
int get_device_num(int device_type)
{
    switch (device_type)
    {
    case 0: // webcam_FHD
        return 2;
    case 2: // webcam_UHD
        return 1;
    case 3: // monitor_50
        return 2;
    case 5: // monitor_75
        return 1;
    case 6: // projector_2K
        return 2;
    case 8: // projector_4K
        return 1;
    case 9: // screen_100
        return 2;
    case 11: // screen_150
        return 1;
    default:
        return -1;
    }
}

// input module

// parseTime function parse the time from two string, one is date, other is time
// the first parameter tm is where time related numbers stored.
// the second parameter date is the string of the date
// the third parameter date is the string of the time
void parseTime(struct tm *tm, char *date, char *time)
{
    int i = 0;

    char temp[strlen(date) + strlen(time)];
    strcpy(temp, date);
    strcat(temp, " ");
    strcat(temp, time);

    //printf("date = %s\n", temp);
    char *error = strptime(temp, "%Y-%m-%d %R", tm);
}

//make a room booking and store in the system
void addRoomBooking(rbm *rbms, booking *booking, cmd *cmds, int type)
{
    booking->type = type;

    //get and store the tenant name
    char temp[TENANT_NAME_LEN];
    strcpy(temp, cmds->tokens[1].token);
    char *temp2;
    temp2 = strstr(temp, "tenant");
    strcpy(booking->caller, temp2);

    //get and store the start time
    parseTime(booking->start_time, cmds->tokens[2].token, cmds->tokens[3].token);

    //get and store the duration
    booking->duration = atof(cmds->tokens[4].token);

    //get and store the number of people
    booking->peopleNum = atoi(cmds->tokens[5].token);

    //get and store the device name
    if (cmds->tokens[6].token != NULL)
    {
        strcpy(booking->deviceA, cmds->tokens[6].token);
    }
    if (cmds->tokens[7].token != NULL)
    {
        strcpy(booking->deviceB, cmds->tokens[7].token);
    }

    rbms->bookingSize += 1;
}

//make a addDevice booking
void bookDevice(rbm *rbms, booking *booking, cmd *cmds)
{
    //make sure token[5](device name) is not null
    assert(cmds->tokens[5].token != NULL);
    char *tenantName;
    booking->type = 4;
    char temp[TENANT_NAME_LEN];
    //get and store the tenant name
    strcpy(temp, cmds->tokens[1].token);
    char *temp2;
    temp2 = strstr(temp, "tenant");
    strcpy(booking->caller, temp2);

    //get and store the start time
    parseTime(booking->start_time, cmds->tokens[2].token, cmds->tokens[3].token);

    //get and store the duration
    booking->duration = atof(cmds->tokens[4].token);

    //get and store the device name
    strcpy(booking->deviceA, cmds->tokens[5].token);

    rbms->bookingSize += 1;
}

// output

// Print the report to tht file
void output_report(rbm *rbms, FILE *output, char *algorithm_type)
{
    //to make sure there are enough memory to create two float array
    float *room_occupied_duration = (float *)calloc(ROOM_NUM, sizeof(float));
    if (room_occupied_duration == NULL)
    {
        printf("no memory\n");
        exit(1);
    }

    float *device_occupied_duration = (float *)calloc(DEVICE_NUM, sizeof(float));
    if (device_occupied_duration == NULL)
    {
        printf("no memory\n");
        exit(1);
    }

    //print the header
    fprintf(output, "  For %s:\n", algorithm_type);

    //initialize the array of storing the duration of used time of each room
    int room_no;
    for (room_no = 0; room_no < ROOM_NUM; room_no++)
    {
        room_occupied_duration[room_no] = 0;
    }

    //initialize the array of storing the duration of used time of each device
    int device_no;
    for (device_no = 0; device_no < DEVICE_NUM; device_no++)
    {
        device_occupied_duration[device_no] = 0;
    }

    //get the duration of used time of each room and device, and the number of bookings assigned and rejected, and the number of invalid bookings
    int booking_no, asg_no = 0, rej_no = 0, invalid_no = 0;
    for (booking_no = 0; booking_no < rbms->bookingSize; booking_no++)
    {
        booking *booking = rbms->bookings + booking_no;
        //count the number of booking accepted/rejected and invalid booking
        switch (booking->accepted)
        {
        case 1:
            asg_no++;
            break;
        case 0:
            rej_no++;
            break;
        case -1:
            invalid_no++;
            break;
        }

        //count the used time of each room and device
        if (booking->accepted > 0)
        {

            if (booking->roomNum != -1)
            {
                room_occupied_duration[booking->roomNum] += booking->duration;
            }
            if (booking->deviceA_index != -1)
            {
                device_occupied_duration[booking->deviceA_index] += booking->duration;
            }
            if (booking->deviceB_index != -1)
            {
                device_occupied_duration[booking->deviceB_index] += booking->duration;
            }
        }
    }

    fprintf(output, "\t\tTotal Number of Bookings Received: %d\n", rbms->bookingSize);
    fprintf(output, "\t\t      Number of Bookings Assigned: %d (%.2f%%)\n", asg_no, (float)asg_no / rbms->bookingSize * 100);
    fprintf(output, "\t\t      Number of Bookings Rejected: %d (%.2f%%)\n\n", rej_no, (float)rej_no / rbms->bookingSize * 100);

    fprintf(output, "\t\tUtilization of Time Slot:\n");

    //compute and print out the untilization of each room
    for (room_no = 0; room_no < ROOM_NUM; room_no++)
    {
        float util = room_occupied_duration[room_no] / (float)(MAX_BOOKING_DAY * TIMESLOT_PER_DAY) * 100;
        fprintf(output, "\t\t      %-14s- %.2f%%\n", ROOM_NAME[room_no], util);
    }

    //compute and print out the untilization of each device
    for (device_no = 0; device_no < DEVICE_NUM; device_no++)
    {
        float util = device_occupied_duration[device_no] / (float)(MAX_BOOKING_DAY * TIMESLOT_PER_DAY) * 100;
        fprintf(output, "\t\t      %-14s- %.2f%%\n", DEVICE_NAME[device_no], util);
    }

    //print out the number of invalid request
    fprintf(output, "\n\t\tInvalid request(s) made: %d\n\n", invalid_no + rbms->invalid_req);
    free(room_occupied_duration);
    free(device_occupied_duration);
    fflush(output);
}

//  Print the booking to tht file
void output_booking(booking *booking, FILE *output)
{
    char date[DATE_FORMAT_LEN], startTime[TIME_FORMAT_LEN], endTime[TIME_FORMAT_LEN], type[15], room[10], deviceA[15], deviceB[15];

    //get string of date
    memset(date, 0, DATE_FORMAT_LEN);
    strftime(date, DATE_FORMAT_LEN, "%Y-%m-%d", booking->start_time);

    //get string of start time
    memset(startTime, '\0', TIME_FORMAT_LEN);
    strftime(startTime, TIME_FORMAT_LEN, "%R", booking->start_time);

    //get string of end time
    struct tm *temp_time = (struct tm *)malloc(sizeof(struct tm));
    memcpy(temp_time, booking->start_time, sizeof(struct tm));
    temp_time->tm_hour = temp_time->tm_hour + (int)booking->duration;
    memset(endTime, '\0', TIME_FORMAT_LEN);
    strftime(endTime, TIME_FORMAT_LEN, "%R", temp_time);

    //get string of booking type
    switch (booking->type)
    {
    case 3:
        strcpy(type, "Meeting");
        break;
    case 2:
        strcpy(type, "Presentation");
        break;
    case 1:
        strcpy(type, "Conference");
        break;
    default:
        strcpy(type, "*");
    }

    //get string of booking room
    switch (booking->roomNum)
    {
    case 0:
        strcpy(room, "room_A");
        break;
    case 1:
        strcpy(room, "room_B");
        break;
    case 2:
        strcpy(room, "room_C");
        break;
    default:
        strcpy(room, "*");
    }

    //get string of device(es)
    if (strlen(booking->deviceA) == 0)
    {
        fprintf(output, "%-13s%-8s%-8s%-14s%-11s*\n", date, startTime, endTime, type, room);
    }
    else
    {
        fprintf(output, "%-13s%-8s%-8s%-14s%-11s%s\n", date, startTime, endTime, type, room, booking->deviceA);
        fprintf(output, "%-54s%s\n", "", booking->deviceB);
    }
}

//function of printing the appointment schedule
void output_schedule(rbm *rbms, int accepted, char *algorithm_type, FILE *output)
{
    //get the string of accepted
    char *str;
    if (accepted == 1)
    {
        str = "ACCEPTED";
    }
    else
    {
        str = "REJECTED";
    }

    //print the header
    fprintf(output, "*** Room Booking - %s / %s ***\n\n", str, algorithm_type);

    //output for each tenant
    int i;
    for (i = 0; i < TENANT_NUM; i++)
    {
        int tenant_flag = 0, header_flag = 1;

        int booking_no;

        for (booking_no = 0; booking_no < rbms->bookingSize; booking_no++)
        {
            booking *booking = rbms->bookings + booking_no;
            if (strcmp(booking->caller, TENANT_NAME[i]) == 0 && booking->accepted == accepted)
            {
                tenant_flag = 1;
                //print the header once when the tenant have booking to print out
                if (tenant_flag && header_flag)
                {
                    fprintf(output, "Tenant_%c has the following bookings:\n\n", 'A' + i);
                    fprintf(output, "%-13s%-8s%-8s%-14s%-11s%s\n", "Date", "Start", "End", "Type", "Room", "Device");
                    fprintf(output, "===========================================================================\n");
                    header_flag = 0;
                }
                //call function to output detail of the booking
                output_booking(booking, output);
            }
        }
        if (tenant_flag)
            fprintf(output, "\n");
    }
    fprintf(output, "===========================================================================\n");
    fflush(output);
}

// scheduling

// Create pipes for each room and device
void create_pipe(int room_pipes[ROOM_NUM][2][2],
                 int device_pipes[DEVICE_NUM][2][2])
{
    int i, j;

    for (i = 0; i < ROOM_NUM; i++)
    {
        for (j = 0; j < 2; j++)
        {
            if (pipe(room_pipes[i][j]) < 0)
            {
                printf("Pipe creation error\n");
                exit(1);
            }
        }
    }

    for (i = 0; i < DEVICE_NUM; i++)
    {
        for (j = 0; j < 2; j++)
        {
            if (pipe(device_pipes[i][j]) < 0)
            {
                printf("Pipe creation error\n");
                exit(1);
            }
        }
    }
}

// Read data from pipe, and store it to dest
void pipe_receive(char *dest, int read_pipe, int size, int time)
{
    int n;
    int i = 0;
    char buffer[size];
    while (true)
    {
        read(read_pipe, buffer, size);

        strcpy(dest, buffer);
        if (buffer != NULL)
        {
            i++;
        }
        if (i >= time)
        {
            break;
        }
    }
}

// Initialize the content in timetable int array
void init_timetable(int timetable[MAX_BOOKING_DAY * TIMESLOT_PER_DAY])
{
    int i, j;
    for (i = 0; i < MAX_BOOKING_DAY * TIMESLOT_PER_DAY; i++)
    {
        *(timetable + i) = 0;
    }
}

// store the tokenized array in dest from src
void message_tokenize(char dest[MAX_MESSAGE_TOKEN_NUM][MAX_MESSAGE_TOKEN_LEN], char src[MESSAGE_SIZE])
{
    int i = 0;
    char src_buffer[MESSAGE_SIZE];
    char *buf;

    strcpy(src_buffer, src);

    buf = strtok(src_buffer, " ");

    do
    {
        memset(dest[i], '\0', MESSAGE_SIZE);
        strcpy(dest[i], buf);
        buf = strtok(NULL, " ");
        i++;
    } while (buf != NULL && i < MAX_MESSAGE_TOKEN_NUM);
}

// update the timetable to 1, starting from index, and continue for the time specify by the integer from duration
void update_timetable(int timetable[MAX_BOOKING_DAY * TIMESLOT_PER_DAY], int index, int duration)
{
    int dur_no = 0;
    for (dur_no = 0; dur_no < duration; dur_no++)
    {
        timetable[index + dur_no] = 1;
    }
}

// Create childs for handle room booking schedule
void room_schedule_manager(rbm *rbms,
                           int room_childIDs[ROOM_NUM],
                           int room_pipes[ROOM_NUM][2][2])
{
    int i, j;

    int timetable[MAX_BOOKING_DAY * TIMESLOT_PER_DAY];
    init_timetable(timetable);
    int room_no;

    for (room_no = 0; room_no < ROOM_NUM; room_no++)
    {
        room_childIDs[room_no] = fork();
        if (room_childIDs[room_no] < 0)
        {
            printf("Fork failed\n");
            exit(1);
        }
        else if (room_childIDs[room_no] == 0)
        {
            close(room_pipes[room_no][0][1]); // from parent to child
            close(room_pipes[room_no][1][0]); // from child to parent

            char read_buffer[MESSAGE_SIZE];
            char write_buffer[RESPONSE_SIZE];
            char message_tokens[MAX_MESSAGE_TOKEN_NUM][MAX_MESSAGE_TOKEN_LEN];

            int n;
            while (1)
            {
                pipe_receive(read_buffer, room_pipes[room_no][0][0], MESSAGE_SIZE, 1);
                message_tokenize(message_tokens, read_buffer);

                int accept = 1;
                if (atoi(message_tokens[0]) == 1)
                {
                    //printf("deciding one booking \n");
                    int timeslot_index = atoi(message_tokens[1]);
                    int duration = atoi(message_tokens[2]);

                    if (timeslot_index + duration - 1 > MAX_BOOKING_DAY * TIMESLOT_PER_DAY)
                    {
                        accept = 0;
                    }
                    else
                    {
                        int dur_no = 0;
                        for (dur_no = 0; dur_no < duration; dur_no++)
                        {
                            if (timetable[timeslot_index + dur_no] == 1)
                            {
                                accept = 0;
                                break;
                            }
                        }
                    }
                    // output

                    if (accept)
                    {
                        strcpy(write_buffer, "OK\0");
                        write(room_pipes[room_no][1][1], write_buffer, RESPONSE_SIZE);
                    }
                    else
                    {
                        strcpy(write_buffer, "NO\0");
                        write(room_pipes[room_no][1][1], write_buffer, RESPONSE_SIZE);
                    }
                }
                else if (atoi(message_tokens[0]) == 2)
                {
                    update_timetable(timetable, atoi(message_tokens[1]), atoi(message_tokens[2]));
                    strcpy(write_buffer, "OK\0");
                    write(room_pipes[room_no][1][1], write_buffer, RESPONSE_SIZE);
                }
                else if (atoi(message_tokens[0]) == 9)
                {
                    break;
                }
            }

            close(room_pipes[room_no][0][0]); // close read
            close(room_pipes[room_no][1][1]); // close write

            exit(0);
        }
    }
    return;
}

// Create childs for handle device booking schedule
void device_schedule_manager(rbm *rbms,
                             int device_childIDs[DEVICE_NUM],
                             int device_pipes[DEVICE_NUM][2][2])
{
    int i, j;

    int timetable[MAX_BOOKING_DAY * TIMESLOT_PER_DAY];
    init_timetable(timetable);

    int device_no;
    for (device_no = 0; device_no < DEVICE_NUM; device_no++)
    {

        device_childIDs[device_no] = fork();
        if (device_childIDs[device_no] < 0)
        {
            printf("Fork failed\n");
            exit(1);
        }
        else if (device_childIDs[device_no] == 0)
        {
            close(device_pipes[device_no][0][1]); // child read only
            close(device_pipes[device_no][1][0]); // child write only

            char read_buffer[MESSAGE_SIZE];
            char write_buffer[RESPONSE_SIZE];
            char message_tokens[MAX_MESSAGE_TOKEN_NUM][MAX_MESSAGE_TOKEN_LEN];

            int n;
            while (1)
            {

                // input
                pipe_receive(read_buffer, device_pipes[device_no][0][0], MESSAGE_SIZE, 1);
                message_tokenize(message_tokens, read_buffer);

                // processing

                int accept = 1;
                if (atoi(message_tokens[0]) == 1)
                {
                    int timeslot_index = atoi(message_tokens[1]);
                    int duration = atoi(message_tokens[2]);

                    if (timeslot_index + duration - 1 > MAX_BOOKING_DAY * TIMESLOT_PER_DAY)
                    {
                        accept = 0;
                    }
                    else
                    {
                        int dur_no = 0;
                        for (dur_no = 0; dur_no < duration; dur_no++)
                        {
                            if (timetable[timeslot_index + dur_no] == 1)
                            {
                                accept = 0;
                                break;
                            }
                        }
                    }

                    // output

                    if (accept)
                    {
                        strcpy(write_buffer, "OK\0");
                        write(device_pipes[device_no][1][1], write_buffer, RESPONSE_SIZE);
                    }
                    else
                    {
                        strcpy(write_buffer, "NO\0");
                        write(device_pipes[device_no][1][1], write_buffer, RESPONSE_SIZE);
                    }
                }
                else if (atoi(message_tokens[0]) == 2)
                {
                    update_timetable(timetable, atoi(message_tokens[1]), atoi(message_tokens[2]));

                    strcpy(write_buffer, "OK\0");

                    write(device_pipes[device_no][1][1], write_buffer, RESPONSE_SIZE);
                }
                else if (strcmp(message_tokens[0], "9") == 0)
                {
                    break;
                }
            }

            close(device_pipes[device_no][0][0]); // close read
            close(device_pipes[device_no][1][1]); // close write

            exit(0);
        }
    }
    return;
}

// Make the message for booking manageer to send a message to child
void make_message(char *write_buffer, char *request_type, booking *booking, int tt_index)
{
    int i;
    char temp[MESSAGE_SIZE];
    strcpy(write_buffer, request_type);
    strcat(write_buffer, " ");
    sprintf(temp, "%d", tt_index);
    strcat(write_buffer, temp);
    strcat(write_buffer, " ");

    memset(temp, '\0', MESSAGE_SIZE);
    sprintf(temp, "%d", (int)booking->duration);
    strcat(write_buffer, temp);
    // add whitespace
    for (i = 0; i < MESSAGE_SIZE - strlen(write_buffer); i++)
    {
        strcat(write_buffer, "\0");
    }
}

// Send the message to the childs
int request_child(char read_buffer[MESSAGE_SIZE],
                  char write_buffer[RESPONSE_SIZE],
                  int pipes[DEVICE_NUM][2][2],
                  int pipe_no)
{

    memset(read_buffer, '\0', sizeof(char) * strlen(read_buffer));
    write(pipes[pipe_no][0][1], write_buffer, MESSAGE_SIZE);
    while (strcmp(read_buffer, "OK") != 0 && strcmp(read_buffer, "NO") != 0)
    {
        pipe_receive(read_buffer, pipes[pipe_no][1][0], RESPONSE_SIZE, 1);
    }
    if (strncmp(read_buffer, "OK", 2) == 0)
    {
        return 1;
    }
    return 0;
}

// Check whether the first device and second device are the required combination
int is_valid_device_combination(cmd *cmds)
{
    int deviceA_type = get_device_index(cmds->tokens[6].token);
    int deviceB_type = get_device_index(cmds->tokens[7].token);
    int device_combination_approved = 0;
    if (deviceA_type == 0 || deviceA_type == 1 || deviceA_type == 2)
    {
        if (deviceB_type == 3 || deviceB_type == 4 || deviceB_type == 5)
        {
            device_combination_approved = 1;
        }
    }
    else if (deviceB_type == 0 || deviceB_type == 1 || deviceB_type == 2)
    {
        if (deviceA_type == 3 || deviceA_type == 4 || deviceA_type == 5)
        {
            device_combination_approved = 1;
        }
    }

    if (deviceA_type == 6 || deviceA_type == 7 || deviceA_type == 8)
    {
        if (deviceB_type == 9 || deviceB_type == 10 || deviceB_type == 11)
        {
            device_combination_approved = 1;
        }
    }
    else if (deviceB_type == 6 || deviceB_type == 7 || deviceA_type == 8)
    {
        if (deviceA_type == 9 || deviceA_type == 10 || deviceA_type == 11)
        {
            device_combination_approved = 1;
        }
    }

    return device_combination_approved;
}

//find free slot for the booking
void booking_manager(booking *booking, int room_pipes[ROOM_NUM][2][2], int device_pipes[DEVICE_NUM][2][2])
{
    //get the time slot index for start time
    struct tm *first_day = (struct tm *)calloc(1, sizeof(struct tm));
    strptime(FIRST_DAY, "%Y-%m-%d", first_day);
    int dayDiff = booking->start_time->tm_mday - first_day->tm_mday;
    int hourDiff = booking->start_time->tm_hour - first_day->tm_hour;
    int tt_index = dayDiff * TIMESLOT_PER_DAY + hourDiff;

    //creat buffer for pipe
    char write_buffer[MESSAGE_SIZE];
    memset(write_buffer, '\0', MESSAGE_SIZE);
    char read_buffer[MESSAGE_SIZE];

    int j;

    int room_approval = 0;
    int deviceA_approval = 1;
    int deviceB_approval = 1;
    booking->deviceA_index = -1;
    booking->deviceB_index = -1;

    int room_no;

    //find room

    //Device booking
    if (booking->type == 4)
    {
        deviceA_approval = 0;
        int deviceA_index = get_device_index(booking->deviceA);
        if (deviceA_index != -1)
        {
            int device_no;

            for (device_no = 0; device_no < get_device_num(deviceA_index); device_no++)
            {
                //ask the child for free time slot
                make_message(write_buffer, "1", booking, tt_index);
                deviceA_approval = request_child(read_buffer, write_buffer, device_pipes, deviceA_index + device_no);

                //break and store the device index if child said have free slot
                if (deviceA_approval)
                {
                    deviceA_index += device_no;
                    break;
                }
            }
        }
        if (deviceA_approval)
        {
            if (deviceA_index != -1)
            {
                //tell the child to assign the booking to time slot
                make_message(write_buffer, "2", booking, tt_index);

                //wait for child to responce
                request_child(read_buffer, write_buffer, device_pipes, deviceA_index);
                while (strcmp(read_buffer, "OK\0") != 0)
                {
                    pipe_receive(read_buffer, device_pipes[deviceA_index][1][0], RESPONSE_SIZE, 1);
                }

                //store the device index and update the accepted in booking
                booking->deviceA_index = deviceA_index;
                booking->accepted = 1;
            }
        }
    }
    //Room booking
    else
    {
        //room
        for (room_no = 0; room_no < ROOM_NUM; room_no++)
        {
            //cheak if the number of people in booking exceed the room limitation, if yes continue to next room
            bool enoughSpace = booking->peopleNum <= ROOM_CAPACITY[room_no];
            if (!enoughSpace)
            {
                continue;
            }

            //ask the child for free time slot
            make_message(write_buffer, "1", booking, tt_index);
            memset(read_buffer, '\0', sizeof(char) * strlen(read_buffer));
            write(room_pipes[room_no][0][1], write_buffer, MESSAGE_SIZE);

            //wait for child to responce
            while (strcmp(read_buffer, "OK") != 0 && strcmp(read_buffer, "NO") != 0)
            {
                pipe_receive(read_buffer, room_pipes[room_no][1][0], RESPONSE_SIZE, 1);
            }

            //break and store the room number if child said have free slot
            if (strncmp(read_buffer, "OK", 2) == 0)
            {
                room_approval = room_no + 1;
                break;
            }
        }

        //device one
        int deviceA_index = get_device_index(booking->deviceA);
        if (deviceA_index != -1)
        {
            int device_no;

            for (device_no = 0; device_no < get_device_num(deviceA_index); device_no++)
            {
                //ask the child for free time slot
                make_message(write_buffer, "1", booking, tt_index);
                deviceA_approval = request_child(read_buffer, write_buffer, device_pipes, deviceA_index + device_no);

                //break and store the device index if child said have free slot
                if (deviceA_approval)
                {
                    deviceA_index += device_no;
                    break;
                }
            }
        }

        //device two
        int deviceB_index = get_device_index(booking->deviceB);
        if (deviceB_index != -1)
        {
            int device_no;
            for (device_no = 0; device_no < get_device_num(deviceB_index); device_no++)
            {
                //ask the child for free time slot
                make_message(write_buffer, "1", booking, tt_index);
                deviceB_approval = request_child(read_buffer, write_buffer, device_pipes, deviceB_index + device_no);

                //break and store the device index if child said have free slot
                if (deviceB_approval)
                {
                    deviceB_index += device_no;
                    break;
                }
            }
        }

        //if room(and all device) found free slot
        if (room_approval && deviceA_approval && deviceB_approval)
        {
            int temp_no = room_approval - 1;

            //tell the room child to assign the booking to time slot
            make_message(write_buffer, "2", booking, tt_index);
            request_child(read_buffer, write_buffer, room_pipes, temp_no);

            //wait for child to responce
            while (strcmp(read_buffer, "OK\0") != 0)
            {
                pipe_receive(read_buffer, room_pipes[temp_no][1][0], RESPONSE_SIZE, 1);
            }

            //tell the device one child to assign the booking to time slot
            if (deviceA_index != -1)
            {
                make_message(write_buffer, "2", booking, tt_index);
                request_child(read_buffer, write_buffer, device_pipes, deviceA_index);

                //store the device index in booking
                booking->deviceA_index = deviceA_index;

                //wait for child to responce
                while (strcmp(read_buffer, "OK\0") != 0)
                {
                    pipe_receive(read_buffer, device_pipes[deviceA_index][1][0], RESPONSE_SIZE, 1);
                }
            }

            //tell the device two child to assign the booking to time slot
            if (deviceB_index != -1)
            {
                make_message(write_buffer, "2", booking, tt_index);
                request_child(read_buffer, write_buffer, device_pipes, deviceB_index);

                //store the device index in booking
                booking->deviceB_index = deviceB_index;

                //wait for child to responce
                while (strcmp(read_buffer, "OK\0") != 0)
                {
                    pipe_receive(read_buffer, device_pipes[deviceB_index][1][0], RESPONSE_SIZE, 1);
                }
            }

            //store the room number and update the accepted in booking
            if (strncmp(read_buffer, "OK", 2) == 0)
            {
                booking->roomNum = room_approval - 1;
                booking->accepted = 1;
            }
        }
    }
}

// Decide to accept or reject the current bookings based on first come first serve
int fcfs_scheduling(rbm *rbms)
{
    // all pipes;

    int room_childIDs[ROOM_NUM];
    int room_pipes[ROOM_NUM][2][2];

    int device_childIDs[DEVICE_NUM];
    int device_pipes[DEVICE_NUM][2][2];

    create_pipe(room_pipes, device_pipes);
    room_schedule_manager(rbms, room_childIDs, room_pipes);
    device_schedule_manager(rbms, device_childIDs, device_pipes);
    int i, wpid, status;

    if (room_childIDs[0] > 0)
    {
        // close unwanted pipes

        for (i = 0; i < ROOM_NUM; i++)
        {
            close(room_pipes[i][0][0]); // from parent to child
            close(room_pipes[i][1][1]); // from child to parent
        }

        for (i = 0; i < DEVICE_NUM; i++)
        {
            close(device_pipes[i][0][0]); // from parent to child
            close(device_pipes[i][1][1]); // from child to parent
        }

        // parent core code start here

        for (i = 0; i < rbms->bookingSize; i++)
        {
            booking *booking = rbms->bookings + i;
            booking_manager(booking, room_pipes, device_pipes);
        }

        // end child

        for (i = 0; i < ROOM_NUM; i++)
        {
            write(room_pipes[i][0][1], "9\0", MESSAGE_SIZE);
        }

        for (i = 0; i < DEVICE_NUM; i++)
        {
            write(device_pipes[i][0][1], "9\0", MESSAGE_SIZE);
        }

        // close all pipes

        for (i = 0; i < ROOM_NUM; i++)
        {
            close(room_pipes[i][0][1]);
            close(room_pipes[i][1][0]);
        }

        for (i = 0; i < DEVICE_NUM; i++)
        {
            close(device_pipes[i][0][1]);
            close(device_pipes[i][1][0]);
        }
        while ((wpid = wait(&status)) > 0)
            ;
        return;
    }
}

//  Decide to accept or reject the current bookings based on priority
int prio_scheduling(rbm *rbms)
{
    int room_childIDs[ROOM_NUM];
    int room_pipes[ROOM_NUM][2][2];

    int device_childIDs[DEVICE_NUM];
    int device_pipes[DEVICE_NUM][2][2];

    create_pipe(room_pipes, device_pipes);
    room_schedule_manager(rbms, room_childIDs, room_pipes);
    device_schedule_manager(rbms, device_childIDs, device_pipes);
    int i, status, wpid;
    if (room_childIDs[0] > 0)
    {
        // close unwanted pipes

        for (i = 0; i < ROOM_NUM; i++)
        {
            close(room_pipes[i][0][0]); // from parent to child
            close(room_pipes[i][1][1]); // from child to parent
        }

        for (i = 0; i < DEVICE_NUM; i++)
        {
            close(device_pipes[i][0][0]); // from parent to child
            close(device_pipes[i][1][1]); // from child to parent
        }

        // parent core code start here
        int prio;
        int booking_no;

        for (prio = 1; prio < BOOKING_TYPE + 1; prio++)
        {
            for (booking_no = 0; booking_no < rbms->bookingSize; booking_no++)
            {
                booking *booking = rbms->bookings + booking_no;
                if (booking->type == prio)
                {
                    booking_manager(booking, room_pipes, device_pipes);
                }
            }
        }

        // end child

        for (i = 0; i < ROOM_NUM; i++)
        {
            write(room_pipes[i][0][1], "9\0", MESSAGE_SIZE);
        }

        for (i = 0; i < DEVICE_NUM; i++)
        {
            write(device_pipes[i][0][1], "9\0", MESSAGE_SIZE);
        }

        // close all pipes

        for (i = 0; i < ROOM_NUM; i++)
        {
            close(room_pipes[i][0][1]);
            close(room_pipes[i][1][0]);
        }

        for (i = 0; i < DEVICE_NUM; i++)
        {
            close(device_pipes[i][0][1]);
            close(device_pipes[i][1][0]);
        }
        while ((wpid = wait(&status)) > 0)
            ;
    }
}

//print the appointment schedule for FCFS and Priority, and Summary Report for "ALL"
void print_booking_handler(rbm *rbms, cmd *cmds)
{
    int pid;
    FILE *schedule;
    FILE *report;
    char temp[100];
    char temp2[2];

    //open the schedule output file
    strcpy(temp, SCHEDULE_PREFIX);
    sprintf(temp2, "%d.txt", rbms->reportNum);
    strcat(temp, temp2);
    schedule = fopen(temp, "w");

    pid = fork();
    if (pid < 0)
    {
        printf("\n\nFork fail\n\n");
    }
    else if (pid == 0)
    {
        char token[MAX_TOKEN_LENGTH];
        if (cmds->tokenNum > 1)
        {
            ignore_first_char(token, cmds->tokens[1].token, MAX_TOKEN_LENGTH);
            if (strcmp(token, "prio") == 0)
            {
                //call the Priority scheduling fuction
                prio_scheduling(rbms);

                //print out the appointment schedule
                output_schedule(rbms, 1, "PRIO", schedule);
                output_schedule(rbms, 0, "PRIO", schedule);
            }
            else if (strcmp(token, "fcfs") == 0)
            {
                //call the FCFS scheduling fuction
                fcfs_scheduling(rbms);

                //print out the appointment schedule
                output_schedule(rbms, 1, "FCFS", schedule);
                output_schedule(rbms, 0, "FCFS", schedule);
            }
            else if (strcmp(token, "ALL") == 0)
            {
                //open the report output file
                strcpy(temp, REPORT_PREFIX);
                sprintf(temp2, "%d.txt", rbms->reportNum);
                strcat(temp, temp2);
                report = fopen(temp, "w");

                //print the header of Summary report
                fprintf(report, "*** Room Booking Manager – Summary Report ***\n\n");
                fprintf(report, "Performance:\n\n");
                fflush(report);

                //call the scheduling fuction
                prio_scheduling(rbms);

                //print out the appointment schedule of Priority
                output_schedule(rbms, 1, "PRIO", schedule);
                output_schedule(rbms, 0, "PRIO", schedule);

                //print out the Priority part of Summary Report
                output_report(rbms, report, "PRIO");

                //call the FCFS scheduling fuction
                fcfs_scheduling(rbms);

                //print out the appointment schedule of FCFS
                output_schedule(rbms, 1, "FCFS", schedule);
                output_schedule(rbms, 0, "FCFS", schedule);

                //print out the FCFS part of Summary Report
                output_report(rbms, report, "FCFS");

                fclose(report);
            }
        }
        else
        {
            printf("expect option\n");
        }

        fprintf(schedule, "   - End - \n");
        fflush(schedule);
        exit(0);
    }
    else
    {
        wait(NULL);
        fclose(schedule);
    }

    //count the time of schedule or report created
    rbms->reportNum++;
    return;
}

// cmd

// ignore the first char in the src string, copy to the dest pointer
void ignore_first_char(char *dest, char *src, int size)
{
    int i = 0;
    char *temp = (char *)calloc(size, sizeof(char));
    memset(temp, '\0', sizeof(char) * size);
    for (i = 0; i < size - 1; i++)
    {
        *(temp + i) = *(src + i + 1);
    }
    strcpy(dest, temp);
    free(temp);
}

// ignore the last char in the src string, copy to the dest pointer
void ignore_rest_char(char *dest, char *src, int size)
{
    int i = 0;
    char *temp = (char *)calloc(size, sizeof(char));
    memset(temp, '\0', sizeof(char) * size);
    for (i = 0; i < size; i++)
    {
        if (*(src + i) == 13 || *(src + i) == 59)
        {
            break;
        }
        *(temp + i) = *(src + i);
    }
    strcpy(dest, temp);
    free(temp);
}

// clear all tokens in the token array, and create new tokens according to user input.
int tokenizer(cmd *cmds)
{
    int i = 0;
    char *buf;
    char tempUserInput[MAX_USER_INPUT];
    int token_no;
    for (token_no = 0; token_no < cmds->tokenNum; token_no++)
    {

        memset(cmds->tokens[token_no].token, '\0', MAX_TOKEN_LENGTH);
    }

    strcpy(tempUserInput, cmds->userInput);
    buf = strtok(tempUserInput, " ");
    do
    {
        ignore_rest_char(cmds->tokens[i].token, buf, strlen(buf));
        /*if (buf[strlen(buf) - 1] == ';')
        {
            ignore_last_char(cmds->tokens[i].token, buf, strlen(buf));
        }
        else
        {

            strcpy(cmds->tokens[i].token, buf);
        }*/
        buf = strtok(NULL, " ");
        i++;

    } while (buf != NULL && i < MAX_TOKEN);

    cmds->tokenNum = i;
}

// Get the user input from bash
void getCmd(cmd *cmds)
{
    char buf[MAX_USER_INPUT];
    printf("> ");
    scanf(" %[^\n]", buf);

    strcpy(cmds->userInput, buf);
    printf("\n");
}

// get the command type from the first token
int getCmdType(cmd *cmds)
{
    if (strcmp(cmds->tokens[0].token, "addConference") == 0)
    {
        return 1;
    }
    else if (strcmp(cmds->tokens[0].token, "addPresentation") == 0)
    {
        return 2;
    }
    else if (strcmp(cmds->tokens[0].token, "addMeeting") == 0)
    {
        return 3;
    }
    else if (strcmp(cmds->tokens[0].token, "bookDevice") == 0)
    {
        return 4;
    }
    else if (strcmp(cmds->tokens[0].token, "addBatch") == 0)
    {
        return 5;
    }
    else if (strcmp(cmds->tokens[0].token, "printBookings") == 0)
    {
        return 6;
    }
    else if (strcmp(cmds->tokens[0].token, "endProgram") == 0)
    {
        return 7;
    }
    else
    {
        return -1;
    }
}

// check if room booking has valid tenant and device name, return 1 if valid, 0 if invalid
int is_valid_roomBooking(cmd *cmds, int isPresent)
{
    int validTenant = 0, validDeviceA = 0, validDeviceB = 0, i;

    int is_valid_combi = 1;

    //check if the tenant name valid
    if (strlen(cmds->tokens[1].token) == TENANT_NAME_LEN)
    {
        char tenant[TENANT_NAME_LEN];
        ignore_first_char(tenant, cmds->tokens[1].token, TENANT_NAME_LEN);
        for (i = 0; i < TENANT_NUM; i++)
        {
            if (strcmp(TENANT_NAME[i], tenant) == 0)
            {
                validTenant = 1;
                break;
            }
        }
    }

    //print error message if invalid tenant name
    if (validTenant == 0)
    {
        printf("Error: invalid tenant name\n");
    }

    //check if the device name valid

    if (strlen(cmds->tokens[6].token) == 0 && strlen(cmds->tokens[7].token) == 0)
    {
        if (isPresent)
        {
            printf("Error: Missing device name\n");
            return 0;
        }
        validDeviceA = 1;
        validDeviceB = 1;
    }
    else
    {
        const char *DEVICE[] = {
            "webcam_FHD",
            "webcam_UHD",
            "monitor_50",
            "monitor_75",
            "projector_2K",
            "projector_4K",
            "screen_100",
            "screen_150"};
        for (i = 0; i < 8; i++)
        {
            if (validDeviceA == 0 && cmds->tokens[6].token != NULL && strcmp(DEVICE[i], cmds->tokens[6].token) == 0)
            {
                validDeviceA = 1;
            }
            if (validDeviceB == 0 && cmds->tokens[7].token != NULL && strcmp(DEVICE[i], cmds->tokens[7].token) == 0)
            {
                validDeviceB = 1;
            }
        }
        if (validDeviceA == 1 && validDeviceB == 1)
        {
            is_valid_combi = is_valid_device_combination(cmds);
        }
    }

    //print error message if invalid device name

    if (validDeviceA == 0)
    {
        printf("Error: invalid first device name\n");
    }
    if (validDeviceB == 0)
    {
        printf("Error: invalid second device name\n");
    }
    if (is_valid_combi == 0)
    {
        printf("Error: invalid first and second device combination\n");
    }

    if (validTenant == 1 && validDeviceA == 1 && validDeviceB == 1 && is_valid_combi == 1)
        return 1;
    return 0;
}

// check if device booking has valid tenant and device name, return 1 if valid, 0 if invalid
int is_valid_deviceBooking(cmd *cmds)
{
    int validTenant = 0, validDevice = 0, i;

    //check if the tenant name valid
    if (strlen(cmds->tokens[1].token) == TENANT_NAME_LEN)
    {
        char tenant[TENANT_NAME_LEN];
        ignore_first_char(tenant, cmds->tokens[1].token, TENANT_NAME_LEN);
        for (i = 0; i < TENANT_NUM; i++)
        {
            if (strcmp(TENANT_NAME[i], tenant) == 0)
            {
                validTenant = 1;
                break;
            }
        }
    }

    //check if the device name valid
    const char *DEVICE[] = {
        "webcam_FHD",
        "webcam_UHD",
        "monitor_50",
        "monitor_100",
        "projector_2K",
        "projector_4K",
        "screen_100",
        "screen_150"};
    for (i = 0; i < 8; i++)
    {
        if (strcmp(DEVICE[i], cmds->tokens[5].token) == 0)
        {
            validDevice = 1;
            break;
        }
    }

    //print error message if invalid
    if (validTenant == 0)
    {
        printf("Error: invalid tenant name\n");
    }
    if (validDevice == 0)
    {
        printf("Error: invalid device name\n");
    }

    if (validTenant == 1 && validDevice == 1)
        return 1;
    return 0;
}

// Run the program according to the command type
void parseCmd(rbm *rbms, cmd *cmds)
{
    char option[MAX_TOKEN_LENGTH];
    switch (getCmdType(cmds))
    {
    case 1: //addConference
        printf("pending ...\n");
        if (is_valid_roomBooking(cmds, 1) == 0)
        {
            rbms->invalid_req++;
            break;
        }
        addRoomBooking(rbms, rbms->bookings + rbms->bookingSize, cmds, 1);
        break;
    case 2: //addPresentation
        printf("pending ...\n");
        if (is_valid_roomBooking(cmds, 1) == 0)
        {
            rbms->invalid_req++;
            break;
        }
        addRoomBooking(rbms, rbms->bookings + rbms->bookingSize, cmds, 2);
        break;
    case 3: //addMeeting
        printf("pending ...\n");
        if (is_valid_roomBooking(cmds, 0) == 0)
        {
            rbms->invalid_req++;
            break;
        }
        addRoomBooking(rbms, rbms->bookings + rbms->bookingSize, cmds, 3);
        break;
    case 4: //bookDevice
        printf("pending ...\n");
        if (is_valid_deviceBooking(cmds) == 0)
            break;
        bookDevice(rbms, rbms->bookings + rbms->bookingSize, cmds);
        break;
    case 5: //addBatch
        ignore_first_char(option, cmds->tokens[1].token, MAX_TOKEN_LENGTH);
        read_batch_file(rbms, option);
        break;
    case 6: //printBookings
        print_booking_handler(rbms, cmds);
        printf("done\n");
        break;
    case 7: //endProgram
        break;
    default:
        printf("unknown command\n");
    };
}

// Read the batch file
void read_batch_file(rbm *rbms, char *file_name)
{
    cmd *cmds = (cmd *)calloc(1, sizeof(cmd));
    cmd_constructor(cmds);
    if (cmds == NULL)
    {
        printf("Failure to allocate room for the array\n");
        exit(0);
    }
    FILE *stream;
    stream = fopen(file_name, "r");
    if (stream == NULL)
    {
        printf("No such file.\n");
        return;
    }
    int temp_line_index = 0;
    char temp_line[MAX_USER_INPUT];
    char *user_input = malloc(MAX_USER_INPUT);
    while (fscanf(stream, "%[^\n] ", user_input) != EOF)
    {
        printf("> %s\n", user_input);
        strcpy(cmds->userInput, user_input);
        tokenizer(cmds);
        parseCmd(rbms, cmds);
    }
    if (feof(stream))
    {
        printf("end of file\n");
    }
    else
    {
        printf("not end\n");
    }

    fclose(stream);
}

// driver

void main()
{
    int i;

    printf("Welcome to rbm!\n");
    rbm *rbms = (rbm *)calloc(1, sizeof(rbm));
    rbm_constructor(rbms);
    if (rbms == NULL)
    {
        printf("Failure to allocate room for the array\n");
        exit(0);
    }

    cmd *cmds = (cmd *)calloc(1, sizeof(cmd));
    cmd_constructor(cmds);
    if (cmds == NULL)
    {
        printf("Failure to allocate room for the array\n");
        exit(0);
    }

    do
    {
        getCmd(cmds);
        tokenizer(cmds);
        parseCmd(rbms, cmds);
    } while (strcmp(cmds->userInput, "endProgram;") != 0);
    printf("program ended\n");
}
