#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <iostream>
#include <ros/ros.h>
#include <std_msgs/String.h>
using namespace std;

// buffers
char send_buffer[1024];

// locks
pthread_mutex_t m_s_buffer;

// semaphores
pthread_cond_t s_send;

void call_back(const std_msgs::String::ConstPtr& msg, char* buffer, pthread_mutex_t* lock, pthread_cond_t* sema) {
    pthread_mutex_lock(lock);
    if (strlen(buffer) != 0) pthread_cond_wait(sema, lock);
    ROS_INFO("received %s", msg->data.c_str());
    strcpy(buffer, msg->data.c_str());
    pthread_cond_signal(&s_send); // call socket
    pthread_mutex_unlock(lock);
}

void* server_socket(void* arg) {
    //create socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    //bind socket
    struct sockaddr_in myServer;
    myServer.sin_family = AF_INET;
    myServer.sin_port = htons(5401);
    myServer.sin_addr.s_addr = INADDR_ANY;
    int bnd = bind(fd, (struct sockaddr*)&myServer, sizeof(myServer));

    //listen on
    bnd = listen(fd, 128);

    ROS_INFO("Server subscriber ready\n");

    struct sockaddr_in client;
    socklen_t addrlen = sizeof(sockaddr_in);

    // communicate
    while (true) {
        int cnt = accept(fd, (struct sockaddr*)&client, &addrlen);
        char ip[32];
        ROS_INFO("Client: %s connected.\t port: %d \n", inet_ntop(AF_INET, &client.sin_addr.s_addr,  ip, sizeof(ip)), ntohs(client.sin_port));

        // send message
        while (true) {
            pthread_mutex_lock(&m_s_buffer);
            if (strcmp(send_buffer, "") == 0) pthread_cond_wait(&s_send, &m_s_buffer); // if buffer is empty, wait for subscriber
            send(cnt, send_buffer, sizeof(send_buffer), 0); // send message
            ROS_INFO("Message sent: %s\n", send_buffer);
            memset(send_buffer, '\0', sizeof(send_buffer)); // empty the buffer
            pthread_cond_signal(&s_send); // call subscriber
            pthread_mutex_unlock(&m_s_buffer);
        }
    }
    close(fd);
}

int main(int argc, char** argv) {
    //initialize ros
    ros::init(argc, argv, "uav_server_subscriber");
    ros::NodeHandle n;

    //initialize lock
    pthread_mutex_init(&m_s_buffer, NULL);
    
    //initialize semaphore
    pthread_cond_init(&s_send, NULL);

    //initialize buffer
    memset(send_buffer, '\0', sizeof(send_buffer));

    // start socket
    pthread_t socket;
    pthread_create(&socket, NULL, server_socket, NULL);

    // 从topic订阅数据
    ros::Subscriber sub = n.subscribe<std_msgs::String>("uav_message_out", 1000, boost::bind(&call_back, _1, send_buffer, &m_s_buffer, &s_send));
    ros::spin();

    // release resource
    pthread_mutex_destroy(&m_s_buffer);
    pthread_cond_destroy(&s_send);

    return 0;
}