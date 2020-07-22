#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/ioctl.h>

int main(int argc, char *argv[])
{
	FILE* log;/*log파일 변수 선언*/
	log = fopen("./log.txt", "w+");/*log.txt open 파일이 있으면 다시 생성하도록 설정*/
    char dir_path[255];/*입력으로 받은 service 디렉토리*/
    char path[255];/*경로 변수 선언*/
    char *tmp, *tmp2;/*임시 변수*/
    int port;/*입력으로 받은 포트 번호*/
    int from=-1, to=-1;/*total.cgi 파라미터를 받을 변수*/
    int sum =0;/*total.cgi 을 처리하기 위한 sum 변수*/
    char sum_num[100];/*total.cgi을 처리하기 위해 sum을 string형으로 변환 할 때 사용할 변수*/
    char sum_string[300];/*header + sum_num 저장할 변수*/
    char file_name[300];/*log를 남길 때 사용할 file name변수*/
    char log_text[100];/*log 문장을 만들기 위한 변수*/
    int sent;/*sendfile()함수에서 리턴된 파일의 크기 저장 변수*/
    int client;/*소켓 디스크립터 번호가 저장될 int형 변수 선언*/
    struct sockaddr_in client_address;/*sockaddr_in structure 선언*/
    socklen_t sin_len = sizeof(client_address);/*client_address의 size저장 변수*/
    int buf = 2048;/*buffer 크기 설정*/
    char *buffer = malloc(buf);/*문자열 변수인 buffer에 크기 동적 할당 */
    int file;/*파일 전송을 위한 변수*/
    FILE *fp;/*파일 전송을 위한 변수*/
    char fbuf[1024];/*read/send에 사용할 buffer 변수*/
	int byte;/*read/send에 사용할 사이즈를 기록하는 변수*/     

    char gifheader[] ="HTTP/1.1 200 Ok\r\n""Content-Type: image/gif;\r\n\r\n";/*gif헤더*/
    char jpgheader[] ="HTTP/1.1 200 Ok\r\n""Content-Type: image/jpg;\r\n\r\n";/*jpg헤더*/
    char http_header[] ="HTTP/1.1 200 OK\r\n""Content-Type: text/html; charset=UTF-8\r\n\r\n";/*http헤더*/
    char status_404[] = "HTTP/1.1 404 Not Found:\r\n""Content-Type: text/html; charset=UTF-8\r\n\r\n""404 not found\r\n";/*not found헤더*/

    if (argc != 3)/*인자로 service directory와 port번호를 받으면 argc는 총 2개가 된다.*/
    {
        printf("Usage: ./webserver directory port\n");/*형식이 맞지 않으면 사용법을 보여준다.*/
    	exit(1);/*실행을 종료한다.*/
    }
    strcpy(dir_path, argv[1]);/*service directory는 argv[1]에 저장되어 있으므로 dir_path 변수에 넣어준다.*/

    int server = socket(AF_INET, SOCK_STREAM, 0);/*소켓 종류 : AF_INET(인터넷 소켓), 통신방식 : SOCK_STREAM(TCP 사용), server에는 소켓 디스크립터 번호가 int형으로 반환*/
    int on = 1;/*소켓 세부 옵션 설정을 위한 변수*/

    if (server < 0)/*소켓을 생성하지 못한 경우*/
    {
        perror("server");/*에러 메세지 출력*/
        exit(1);/*실행을 종료한다.*/
    }

    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(int));/*소켓 세부 옵션 설정*/
    struct sockaddr_in server_address;/*sockaddr_in structure 선언*/
    server_address.sin_family = AF_INET;/*인터넷 프로토콜*/
    server_address.sin_addr.s_addr = htons(INADDR_ANY);/*모든 클라이언트, 모든 ip로 부터 들어오면 받게 설정*/
    server_address.sin_port = htons(atoi(argv[2]));/*소켓이 사용할 port 지정*/
    
    if (bind(server, (struct sockaddr *)&server_address, sizeof(server_address)) < 0){/*소켓 파일기술자를 지정된 IP 주소, 포트 번호와 결합(소켓에 대해 세팅)*/
        perror("bind");/*bind에 실패하면 에러메세지를 출력*/
        exit(1);/*실행을 종료한다.*/
    }

    /*클라이언트 연결 기다리기*/
    if (listen(server, 256) < 0){/*backlog는 256로 최대 혀용 클라이언트 수는 256이다.*/
        perror("listen");/*listen에 실패하면 에러메세지를 출력한다*/
        exit(1);/*실행을 종료한다.*/
    }
    while (1)/*여러개의 클라이언트를 받아야 하므로 무한 반복한다.*/
    {/*연결 요청 수락하기*/
        client = accept(server, (struct sockaddr *)&client_address, &sin_len);/*client에는 클라이언트 정보가 저장*/
        if (client == -1)/*accept에 실패하면*/
        {
            perror("connection failed");/*에러메세지를 출력한다.*/
        }
        memset(buffer, 0, 1024);/*buffer 초기화*/
        read(client, buffer, 1023);/*client로 부터 buffer을 받는다.*/
        //printf("%s\n\n", buffer);/*디버깅용 buffer 출력*/
        if(!strncmp(buffer, "GET", 3)){/*GET 요청일 때 처리*/
            strtok(buffer, " /");/*파싱*/
            tmp = strtok(NULL, ".");/*tmp에는 /images, /total, /파일명으로 시작하는 문자열로 파싱됨 경우의 수를 처리하기 위한 과정*/
            if(!strncmp(tmp, "/ ", 2)){/*요청하는 파일이 없을 경우(index.html을 전송)*/
            	sprintf(path, "%s/%s", dir_path, "index.html");/*path설정*/
                fp = fopen(path, "r");/*path에 해당하는 파일 open*/
            	//file = open(path, O_RDONLY);/*path에 해당하는 파일 open*/
            	if(fp==NULL){/*file이 없으면*/
                	write(client, status_404, sizeof(status_404)-1);/*404 not found*/
                	sent = sizeof("404 Not Found");/*sent에 404 not found 크기 저장*/
    				sprintf(file_name, "404 not found");/*log용 file_name 설정*/
    			}
            	else{/*file이 있으면*/
	            	write(client, http_header, sizeof(http_header) - 1);/*http헤더*/
            		file = fileno(fp);/*file디스크립터 받기*/
            		while((byte = read(file, fbuf, 1024))>0){/*read*/
            			send(client, fbuf, byte, 0);/*send*/
            		}
	            	sprintf(file_name, "%s","/index.html");/*log용 file_name 설정*/
	            	fseek(fp,0,SEEK_END);/*log용 파일 크기 구하기위해 fseek 으로 파일 끝으로 옮김*/
        			sent = ftell(fp);/*log용 파일 크기 구하기*/
	            	close(file);/*file close*/
        		}
            }

            else if(!strncmp(tmp, "/images", 6)){/*tmp가 /images로 시작할 경우*/
            	tmp2 = strtok(NULL, " ");/*확장자명 파싱*/
                sprintf(path, "%s%s.%s", dir_path, tmp,tmp2);/*path 설정*/
        		fp = fopen(path, "r");/*path에 해당하는 파일 open*/
                if(fp==NULL){/*file이 없으면*/
	                write(client, status_404, sizeof(status_404)-1);/*404 not found*/
                    sent = sizeof("404 not found");/*sent에 404 not found 크기 저장*/
        			sprintf(file_name, "404 not found");/*log용 file_name 설정*/
                }
                else{
	                if(!strcmp(tmp2,"gif"))/*확장자명이 gif일 때*/
	                    write(client, gifheader, sizeof(gifheader) - 1);/*gif헤더로 write*/
	                if(!strcmp(tmp2,"jpg"))/*확장자명이 gif일 때*/
	                    write(client, jpgheader, sizeof(jpgheader) - 1);/*jpg헤더로 write*/
	                file = fileno(fp);/*file디스크립터 받기*/
            		while((byte = read(file, fbuf, 1024))>0){/*read*/
            			send(client, fbuf, byte, 0);/*send*/
            		}
	                sprintf(file_name, "%s.%s",tmp, tmp2);/*log용 file_name 설정*/
	                fseek(fp,0,SEEK_END);/*log용 파일 크기 구하기위해 fseek 으로 파일 끝으로 옮김*/
        			sent = ftell(fp);/*log용 파일 크기 구하기*/
	                close(file);/*file close*/
            	}

            }

            else if(!strncmp(tmp, "/total", 6)){/*tmp가 /total 로 시작할 경우*/
            	if(!strncmp(strtok(NULL,"?"),"cgi",3)){/*확장자명이 cgi임을 검사*/
                    if(!strncmp(strtok(NULL, "="),"from",4)){;/*형식 검사*/
                        from = atoi(strtok(NULL, "&"));/*from 파라미터 파싱 및 int로 형 변환*/
                        strtok(NULL, "=");/*파라미터를 받기 위한 파싱*/
                        to = atoi(strtok(NULL, " "));/*to 파라미터 파싱 및 int로 형 변환*/
                        sprintf(file_name, "%s.cgi?from=%d&to=%d",tmp,tmp2,from,to);/*log용 file_name 설정*/
                        for(int i=from;i<=to;i++){/*from에서 to까지 누적 값 계산*/
                            sum += i;/*sum에 저장 */
                        }
                        sprintf(sum_num, "%d", sum);/*sum을 string형으로 형변환*/
                        sprintf(sum_string, "%s%d\r\n", http_header,sum);/*전송을 위한 헤더 + sum 문자열*/
                        write(client, sum_string, strlen(sum_string));/*전송*/
                        sent = strlen(sum_num);/*전송한 파일 크기 저장 */
                        sum=0;/*sum 초기화*/
                    }
                    else{
                        write(client, status_404, sizeof(status_404)-1);/*total.cgi가 아니면 not found*/
                        sent = sizeof("404 not found");/*sent에 404 not found 크기 저장*/
                        sprintf(file_name, "404 not found");/*log용 file_name 설정*/
                    }
	                
        		}
        		else{
					write(client, status_404, sizeof(status_404)-1);/*total.cgi가 아니면 not found*/
                    sent = sizeof("404 not found");/*sent에 404 not found 크기 저장*/
        			sprintf(file_name, "404 not found");/*log용 file_name 설정*/
        		}
            }
            else{/*이외의 경우*/
            	tmp2 = strtok(NULL, " ");/*경로 설정을 위한 파싱*/
                sprintf(path, "%s%s.%s", dir_path, tmp,tmp2);/*path 설정*/
                fp = fopen(path, "r");/*path에 해당하는 파일 open*/
                if(fp==NULL){/*file이 없으면*/
                    write(client, status_404, sizeof(status_404)-1);/*404 not found*/
                    sent = sizeof("404 not found");/*sent에 404 not found 크기 저장*/
    				sprintf(file_name, "404 not found");/*log용 file_name 설정*/
                }
                else{
	                if(!strcmp(tmp2,"html")||!strcmp(tmp2,"htm"))/*확장자명이 html, htm일 때*/
	                    write(client, http_header, sizeof(http_header) - 1);/*http헤더로 write*/
	                if(!strcmp(tmp2,"jpg"))/*확장자명이 jpg일 때*/
	                    write(client, jpgheader, sizeof(jpgheader) - 1);/*jpg헤더로 write*/
	                file = fileno(fp);/*파일 디스크립터 구하기*/
            		while((byte = read(file, fbuf, 1024))>0){/*read*/
            			send(client, fbuf, byte, 0);/*read에 성공하면 send*/
            		}
	                fseek(fp,0,SEEK_END);/*log용 파일 크기 구하기위해 fseek 으로 파일 끝으로 옮김*/
        			sent = ftell(fp);/*log용 파일 크기 구하기*/
	                sprintf(file_name, "%s.%s",tmp, tmp2);/*log용 파일 저장*/
	                close(file);/*file close*/
            	}
            }
        }
        sprintf(log_text,"%d.%d.%d.%d %s %d\n", (int)client_address.sin_addr.s_addr&0xFF, (int)(client_address.sin_addr.s_addr&0xFF00)>>8, (int)(client_address.sin_addr.s_addr&0xFF0000)>>16, (int)(client_address.sin_addr.s_addr&0xFF000000)>>24,file_name,sent);/*log text 만들기*/
        fprintf(log, log_text, strlen(log_text));/*log파일 출력*/
        close(client);/*client close*/
    }
    fclose(log);/*log close*/
    fclose(fp);/*fp close*/
    return 0;/*return*/
}