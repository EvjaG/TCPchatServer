#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <signal.h>

#define MAX_READ 4000

/**************************
        VARIABLES
**************************/
int numOfClients=0;

typedef struct msg{
        char* message;
        struct msg *next;
        int origin;
}msg;

typedef struct {
        int fd;
        msg *messages;
}client;

typedef struct {
        int mainSock;
        client** clist;
        int clist_size;
        msg* messages;
}allTheInfo;

allTheInfo ATF;
/**************************
        DECLARATIONS
**************************/
//main functions

char* readFromClient(int readFrom);
int writeToClients(client* client_);
void addToMessagePool(msg *root, char* text,int origin);
void killAll(int i);



//side/assist functions
void error(char* msg,void* data);
int strlen2(char* data);
void strcpy2(char* a, char* b);
void FD_CPY(int maxfd, fd_set *to, fd_set *from);
int killClient(int fd, client** cList);
int increaseClients(int maxfd,client** cList);
msg* createHead(char* text, int origin);

/**************************
*        FUNCTIONS        *
***************************/


/*************************
        MAIN OPERATIONS
**************************/
char* readFromClient(int readFrom)
{
        char* message = (char*)calloc(4001,sizeof(char));
        int rc=read(readFrom,message,4001);
        if(rc<0)
        {
                sprintf(message,"ERROR");
        } else if(rc==0)
        {
                sprintf(message,"FINISH");
        }
        int len = strlen2(message);
        char* message2 = (char*)calloc(len+1,sizeof(char));
        sprintf(message2,"%s",message);
        free(message);


        return message2;
}
int writeToClients(client* client_)
{
        while(client_->messages!=NULL)
        {
                msg* ptr = client_->messages;
                int sLen = strlen2(ptr->message)+100;
                char* msgW = (char*) calloc(sLen,sizeof(char));
                if(client_->fd != ptr->origin){
                        sprintf(msgW,"guest%d: %s",ptr->origin,ptr->message);
                        int wrd = write(client_->fd,msgW,sLen);
                        if( wrd< 0)
                        {
                                free(ptr->message);
                                free(msgW);
                                return -1;
                        }
                }
                client_->messages=client_->messages->next;
                free(msgW);
                free(ptr->message);
                free(ptr);
        }

        return 0;
}
void addToMessagePool(msg *root, char* text,int origin)
{
        if(root->next!=NULL) 
                addToMessagePool(root->next,text,origin);
        else
        {
                int lineStart = 0,i=0;
                for(i=0;text[i]!='\0';i++)
                {//check for new line
                        if(text[i]=='\n')
                        {
                                char* string = (char*)calloc(i-lineStart+15,sizeof(char));
                                int k=0;
                                for(int j=lineStart;j<=i;j++,k++)
                                {
                                        string[k]=text[j];
                                }
                                string[k]='\0';
                                msg* new = (msg*)calloc(1,sizeof(msg));
                                new->message=string;
                                new->origin=origin;
                                root->next=new;
                                root=root->next;
                                lineStart=i+1;
                        }
                }
                if(i==lineStart)
                        return;
                char* string = (char*)calloc(i-lineStart+15,sizeof(char));
                int k=0;
                for(int j=lineStart;j<i;j++,k++)
                {
                        string[k]=text[j];
                }
                string[k]='\0';
                msg* new = (msg*)calloc(1,sizeof(msg));
                new->message=string;
                new->origin=origin;
                root->next=new;
                root=root->next;
                lineStart=i+1;  
                
        }
}
void killAll(int exitcode)
{
        printf("killing all!\n");
        close(ATF.mainSock);
        msg* ptr = ATF.messages;
        while(ptr!=NULL)
        {
                msg* ptr2 = ptr;
                free(ptr2);
                ptr=ptr->next;
        }
        for(int i=0;i<ATF.clist_size;i++)
        {
                client* client_ = ATF.clist[i];
                if(client_==NULL)
                        continue;
                close(client_->fd);
                msg* ptr = client_->messages;
                while(ptr!=NULL)
                {
                        msg* ptr2 = ptr;
                        free(ptr2);
                        ptr=ptr->next;
                }
                free(client_);
        }
        free(ATF.clist);
        printf("killed all!\n");
        exit(exitcode);
}

/*************************
    SECONDARY OPERATIONS
**************************/
int strlen2(char* data)
{
        if(data==NULL) return 0;
        int toReturn=0;
        for(;data[toReturn]!='\0';toReturn++);
        return toReturn+1;
}
void strcpy2(char* a, char* b)
{
        if(b==NULL)
        {
                a[0]='\0';
        } else {
                for(int i=0;b[i]!='\0';i++)
                        a[i]=b[i];
        }
        return;
}
void error(char* msg,void* data)
{
        char* errorMSG = (char*)calloc(strlen2(msg)+2,sizeof(char));
        sprintf(errorMSG,"%s\n",msg);
        free(errorMSG);
        return;
}
void FD_CPY(int maxfd, fd_set *to, fd_set *from)
{
    if(to==NULL || from == NULL)
    {
            printf("error on copy\n");
            exit(1);
    }
    for(int i=0;i<maxfd;i++)
    {
        if(FD_ISSET(i,from))
                FD_SET(i,to);
    }
}
int increaseClients(int maxfd,client** cList)
{
        if(cList==NULL)
                return -1;
        cList=(client**)realloc(cList,maxfd);
        if(cList==NULL)
                return -1;
        return 0;
}
int killClient(int fd, client** cList)
{
        if(cList==NULL || cList[fd] == NULL)
                return -1;
        msg* ptr = cList[fd]->messages;
        while(ptr != NULL)
        {
                msg* ptr2=ptr;
                ptr=ptr->next;
                free(ptr2);
        }
        client* client_= cList[fd];
        close(client_->fd);
        return 0;
}

msg* createHead(char* text,int origin)
{
        if(text==NULL)
                return NULL;
        //create first message in pool
        int lineStart = 0,i=0;
        for(i=0;text[i]!='\0';i++)
                if(text[i]=='\n')
                        break;
        if(i==lineStart)
                return NULL;
        char* string = (char*)calloc(i+20,sizeof(char));
        strncpy(string,text,i+1);
        msg* new = (msg*)calloc(1,sizeof(msg));
        new->message=string;
        new->origin=origin;
        i++;
        if(text[i]!='\0' && text[i+1]!='\0')
        {
                int len = strlen2(text);
                char* theRest = (char*)calloc(len,sizeof(char));
                for(int k=0;text[i]!='\0';k++,i++)
                        theRest[k]=text[i];
                addToMessagePool(new,theRest,origin);
                free(theRest);
        }
        return new;                
}

/*************************
*          MAIN          *
**************************/

int main(int argc, char* argv[]){
    if(argc!=2){
        printf("Usage: \"./server <port>\"\n");
        exit(1);
    }
    int port = atoi(argv[1]);
    char numCheck[6];
    sprintf(numCheck,"%d",port);
    if(port <= 0 || strcmp(argv[1],numCheck) != 0){
        printf("Usage: \"./server <port>\"\n");
        exit(1);
    }
    struct sockaddr_in serv_addr={ 0 };
    struct sockaddr cli_addr= { 0 };
    socklen_t clilen=0;
    memset(&cli_addr,0,sizeof(cli_addr));
    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
            error("ERROR opening socket",NULL);
            return 1;
    }
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_addr.s_addr=INADDR_ANY;
    serv_addr.sin_port=htons(port);
    if( bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0){
        error("ERROR on binding",NULL);
        return 1;
    }
    listen(sockfd,0);
    fd_set readfds;
    fd_set writefds;
    fd_set ogFDs;
    msg *headOfMain=NULL;
    int retval=0;
    int maxfd=sockfd+1;
    int maxfd2=maxfd;
    client** clist = (client**)calloc(1,sizeof(client));
    int clist_size=1;
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    FD_ZERO(&ogFDs);
    FD_SET(sockfd,&ogFDs);
    ATF.clist=clist;
    ATF.mainSock=sockfd;
    ATF.clist_size=1;
    if(signal(SIGINT,killAll) == SIG_ERR || signal(SIGTERM,killAll) == SIG_ERR ){
            printf("CAN'T CATCH SIGINT\n");
            free(clist);
            close(sockfd);
    }
    readfds=ogFDs;

    while(1)
    {
        FD_ZERO(&writefds);
        FD_ZERO(&readfds);
        // FD_SET(sockfd,&readfds);
        FD_CPY(maxfd,&readfds,&ogFDs);
        FD_CPY(maxfd,&writefds,&ogFDs);
        retval = select(maxfd+1,&readfds,&writefds,NULL,NULL);
        

        //check if needs to add to ogFDs
        if(FD_ISSET(sockfd,&readfds))
        {
                int newsockfd=accept(sockfd,&cli_addr,&clilen);
                FD_SET(newsockfd,&ogFDs);
                if(newsockfd>=maxfd2)
                {
                        maxfd2=newsockfd+1;
                }
                //add to client list!
                int avb=-1;
                for(int i=0;i<clist_size;i++)
                        if(clist[i]==NULL)
                        {avb=i;break;}
                if(avb==-1)
                {
                        clist=(client**)realloc(clist,(clist_size+1)*sizeof(client*));
                        ATF.clist=clist;
                        avb=clist_size;
                        clist_size++;
                        ATF.clist_size++;
                }
                clist[avb]=(client*)calloc(1,sizeof(client));
                clist[avb]->fd=newsockfd;
        }
        
        //check if needs to read
        for(int i=0;i<clist_size;i++)
        {
                if(clist[i]==NULL)
                        continue;
                if(FD_ISSET(clist[i]->fd,&readfds))
                {
                        //if you do need to read from it, use the function
                        printf("server is ready to read from socket %d\n",clist[i]->fd);
                        char* message = readFromClient(clist[i]->fd);
                        if(strcmp(message,"FINISH\r\n")==0 || strcmp(message,"FINISH")==0)
                        {
                                int kill=clist[i]->fd;
                                FD_CLR(kill,&writefds);
                                FD_CLR(kill,&readfds);
                                FD_CLR(kill,&ogFDs);
                                killClient(i,clist);
                        }
                        else if(strcmp(message,"ERROR")==0)
                        {
                                char errormsg[150];
                                sprintf(errormsg,"ERROR ON READING FROM FD %d\n",clist[i]->fd);
                                error(errormsg,NULL);
                                free(message);
                                killAll(1);
                        }
                        if(headOfMain!=NULL)//then check if a message queue is available
                                addToMessagePool(headOfMain,message,i);
                        else
                        {//if it isn't, create it and add message to it
                                if(strcmp(message,"FINISH\r\n") == 0 || strcmp(message,"ERROR") == 0 || strcmp(message,"FINISH")==0)
                                {
                                        free(message);
                                        continue;
                                }
                                headOfMain = createHead(message,clist[i]->fd);
                                free(message);
                        }
                                
                }

        }

        //add message to all fds
        msg* ptr = headOfMain;
        while(ptr!=NULL)
        {
                int messagelength=strlen2(ptr->message);
                for(int i=0;i<clist_size;i++)
                {
                        if(clist[i]==NULL || ptr->origin==clist[i]->fd)
                                continue;
                        if(clist[i]->messages==NULL)
                        {
                                clist[i]->messages=(msg*)calloc(1,sizeof(msg));
                                clist[i]->messages->message=(char*)calloc(messagelength,sizeof(char));
                                strcpy(clist[i]->messages->message,ptr->message);
                                clist[i]->messages->origin=ptr->origin;
                        } else {
                                addToMessagePool(clist[i]->messages,ptr->message,ptr->origin);
                        }
                }


                msg* ptr2=ptr;
                ptr=ptr->next;
                headOfMain=headOfMain->next;
                free(ptr2->message);
                free(ptr2);
                ATF.messages=headOfMain;
        }

        //check if you can write to client, and if you can, write to them
        for(int i=0;i<clist_size;i++)
        {
                if( (clist[i]) == NULL)
                        continue;
                int canWrite =  FD_ISSET(clist[i]->fd,&writefds);
                if(!canWrite || clist[i]->messages==NULL)
                        continue;
                printf("server is ready to write to socket %d\n",clist[i]->fd);
                writeToClients(clist[i]);
        }

        //check for killed connections
        char toCheckClose[10];
        // sprintf(toCheckClose,"check\n");s
        for(int i=0;i<clist_size;i++)
        {
                if(clist[i]==NULL)
                        continue;
                if( (write(clist[i]->fd,toCheckClose,0)) < 0)
                {
                        int kill=clist[i]->fd;
                        killClient(i,clist);
                        FD_CLR(kill,&writefds);
                        FD_CLR(kill,&readfds);
                        FD_CLR(kill,&ogFDs);
                }
        }


        maxfd=maxfd2;
    }


    return 0;
}