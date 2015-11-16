/*****************************************************************************
 * @Brief     ROS-free and MavLink-free low-level socket communicator class
 * @Version   1.0
 * @Author    Chris Liu
 * @Created   2015/10/29
 * @Modified  2015/11/14
 *****************************************************************************/

#ifndef _DJI2MAV_SOCKETCOMM_H_
#define _DJI2MAV_SOCKETCOMM_H_


#include "config.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <iostream>
#include <string>

namespace dji2mav {

    class SocketComm {
        public:
            /* Lazy Mode Singleton. WARNING: Unsafe for multi-thread */
            static SocketComm* getInstance() {
                if(NULL == m_instance) {
                    try {
                        m_instance = new SocketComm();
                    } catch(std::bad_alloc &m) {
                        std::cerr << "Cannot new instance of SocketComm: " 
                                << "at line: " << __LINE__ << ", func: " 
                                << __func__ << ", file: " << __FILE__ 
                                << std::endl;
                        perror( m.what() );
                        exit(EXIT_FAILURE);
                    }
                }
                return m_instance;
            }


            /* Set the Connection Configuration */
            void setConf(std::string gcsIP, int gcsPort, int locPort) {
                memset(&m_locAddr, 0, sizeof(m_locAddr));
                m_locAddr.sin_family = AF_INET;
                m_locAddr.sin_addr.s_addr = INADDR_ANY;
                m_locAddr.sin_port = htons(locPort);

                memset(&m_gcsAddr, 0, sizeof(m_gcsAddr));
                m_gcsAddr.sin_family = AF_INET;
                m_gcsAddr.sin_addr.s_addr = inet_addr(gcsIP.c_str());
                m_gcsAddr.sin_port = htons(gcsPort);
            }


            /* Connect to the GCS */
            bool connect() {
                printf("Attempt to connect to %s:%u using local port %u\n", 
                        inet_ntoa(m_gcsAddr.sin_addr), 
                        ntohs(m_gcsAddr.sin_port), 
                        ntohs(m_locAddr.sin_port) );

                // Set socket properties. Using UDP
                if((m_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
                    perror("Socket invalid");
                    return false;
                }

                // Set the Communication Nonblocking
                int flag = fcntl(m_sock, F_GETFL, 0);
                if(fcntl(m_sock, F_SETFL, flag | O_NONBLOCK) < 0) {
                    perror("Set nonblocking socket fail");
                    close(m_sock);
                    return false;
                }

                // Bind socket
                if( -1 == bind(m_sock, (struct sockaddr *)&m_locAddr, 
                        sizeof(struct sockaddr_in)) ) {
                    perror("Bind socket fail");
                    close(m_sock);
                    return false;
                }

                printf("Connection Succeed!\n");
                return true;
            }


            /* Disconnect to the GCS */
            bool disconnect() {
                if(m_sock != -1) {
                    close(m_sock);
                    m_sock = -1;
                    return true;
                } else {
                    return false;
                }
            }


            /**
             * @brief  Send Message Nonblocked
             * @param  sendBuf : pointer to the send buffer
             * @param  msglen  : length of message that is going to be sent
             * @return Bytes that is sent. Return -1 if it fails
             */
            int send(uint8_t* sendBuf, uint16_t msglen) {
                // Using Nonblocking flag "MSG_DONTWAIT"
                int ret = sendto( m_sock, sendBuf, msglen, MSG_DONTWAIT, 
                        (struct sockaddr *)&m_gcsAddr, 
                        sizeof(struct sockaddr_in) );
                if(ret == -1)
                    perror("Fail to send message");
                else if(ret < msglen)
                    perror("Partial msg failed to send");
                return ret;
            }


            /**
             * @brief  Receive Message Nonblocked
             * @param  recvBuf   : pointer to the receive buffer
             * @param  maxBufLen : max length of of the buffer
             * @return Bytes that is received. Return -1 if it fails
             */
            int recv(void* recvBuf, uint16_t maxBufLen) {
                socklen_t l = sizeof(m_gcsAddr);
                // Using Nonblocking flag "MSG_DONTWAIT"
                int ret = recvfrom( m_sock, recvBuf, maxBufLen, MSG_DONTWAIT, 
                        (struct sockaddr *)&m_gcsAddr, &l );
                if(ret == -1) {
                    if(errno != EAGAIN)
                        perror("Fail to receive message");
                    else
                        printf("No datagram received.\n");
                }
                return ret;
            }


        private:
            SocketComm() {
                m_sock = -1;
            }


            ~SocketComm() {
                disconnect();
            }


            static SocketComm* m_instance;
            struct sockaddr_in m_gcsAddr;
            struct sockaddr_in m_locAddr;
            int m_sock;
    };

    SocketComm* SocketComm::m_instance = NULL;

}


#endif
