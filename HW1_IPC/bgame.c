#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "logging.h"
#include "message.h"
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/wait.h>

#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, fd)
#define BOMBER_PLACE -5
#define BOMB_PLACE -10
#define EMPTY_PLACE 0
#define REAP_BOMBER_MSG "Bomber Reap" 
#define REAP_BOMB_MSG "Bomb Reap" 

typedef struct Bomber {
	bool is_alive;
	bool is_died_this_turn;
	coordinate position;
	int pid;
	int file_no;
} Bomber;

typedef struct Bomb {
	bool is_exploded;
	long interval;
	int radius;
	coordinate position;
	int pid;
	int file_no;
} Bomb;

int **maze; /*0 ->  empty,-5 -> bomber , -10 -> bomb, -1 -> unbreakable,else -> durability*/
Bomber *bombers=NULL;
Bomber *winner=NULL;
struct pollfd fds_bombers[1024];
struct pollfd fds_bombs[1024];
Bomb *bombs=NULL;
int total_bomb_count=0;
int maze_width,maze_height,obstacle_count,bomber_count,alive_bombers;
int informed_bomber=0;

void forkBomber(int bomber_index,char** args);
void forkBomb(int bomb_index);
void reap(int pid,char *msg);

void serveRequestBomber(im *msg,int bomber_index);
om bomberStartOm(int bomber_index);
om bomberMoveOm(int bomber_index,int newX,int newY);
om bomberPlantOm(int bomber_index,int radius,long interval);
om bomberSeeOm(int bomber_index,od *objects);
om bomberDieOm(int bomber_index);

void explode(im *msg,int bomb_index);
void check_position_for_exp(int x,int y, bool *is_blocked);
void inputs();

/******FORK********/
void forkBomber(int bomber_index,char** args)
{
    int fd[2];

    int pid;
	if(PIPE(fd)<0)
    {
        // perror("pipe error");
		exit(1);
    }
	bombers[bomber_index].file_no = fd[0];
	
	fds_bombers[bomber_index].fd=bombers[bomber_index].file_no;
	fds_bombers[bomber_index].events=POLLIN;

	if ((bombers[bomber_index].pid = fork())< 0) {
        // perror("fork error");
		exit(1);
	}

	if (bombers[bomber_index].pid == 0) { /*CHILD*/
		close(fd[0]);
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		
		dup2(fd[1], STDOUT_FILENO);
		dup2(fd[1], STDIN_FILENO);
		
		close(fd[1]);
		if(execvp(args[0], args))
		{
			// perror("bomber exec");
		}

	} else { /*PARENT*/
		close(fd[1]);
	}
}
void forkBomb(int bomb_index)
{
    int fd[2];

    int pid;
	char interval[20];

	sprintf(interval,"%ld",bombs[bomb_index].interval);
	
	char *args[]={"./bomb",interval,NULL};

	if(PIPE(fd)<0)
    {
        // perror("pipe error");
		exit(1);
    }
	bombs[bomb_index].file_no = fd[0];
	
	fds_bombs[bomb_index].fd=bombs[bomb_index].file_no;
	fds_bombs[bomb_index].events=POLLIN;

	if ((bombs[bomb_index].pid = fork())< 0) {
        // perror("fork error");
		exit(1);
	}

	if (bombs[bomb_index].pid == 0) { /*CHILD*/
		close(fd[0]);
		close(STDIN_FILENO);
		close(STDOUT_FILENO);
		
		dup2(fd[1], STDOUT_FILENO);
		dup2(fd[1], STDIN_FILENO);
		
		close(fd[1]);
		if(execvp(args[0], args) == -1)
		{
			// perror("execbomb");
		}

	} else { /*PARENT*/
		close(fd[1]);
	}
}
void reap(int pid,char *msg)
{
	int exitStatus;
	waitpid(pid,&exitStatus,0);
	if (WIFEXITED(exitStatus)) {
		// printf("%s:  pid: %d exited with status %d\n",msg,pid, WEXITSTATUS(exitStatus));
	}
	else{
		// perror(msg);
	}	
}
/******BOMBER COMMANDS********/
om bomberStartOm(int bomber_index){

	om send_msg;
	send_msg.type=BOMBER_LOCATION;
	send_msg.data.new_position.x=bombers[bomber_index].position.x;
	send_msg.data.new_position.y=bombers[bomber_index].position.y;
	return send_msg;
}
om bomberMoveOm(int bomber_index,int newX,int newY){

	om send_msg;
	send_msg.type=BOMBER_LOCATION;
	/*if it can not move old pos should sent*/
	send_msg.data.new_position.x=bombers[bomber_index].position.x;
	send_msg.data.new_position.y=bombers[bomber_index].position.y;
	
	bool is_out_of_bounds = newX >= maze_width || newX < 0 || newY >= maze_height || newY < 0;
	if(!is_out_of_bounds)
	{
		int stepX = bombers[bomber_index].position.x - newX;
		int stepY = bombers[bomber_index].position.y - newY;
		
		stepX = stepX < 0 ? -stepX:stepX;
		stepY = stepY < 0 ? -stepY:stepY;
		
		
		bool is_1_step = (stepX+stepY)==1;
		/*empty or bomb*/
		bool is_coord_empty = maze[newY][newX]==EMPTY_PLACE || maze[newY][newX]== BOMB_PLACE;
		if(is_1_step && is_coord_empty)
		{
			maze[bombers[bomber_index].position.y][bombers[bomber_index].position.x]-=BOMBER_PLACE;
			bombers[bomber_index].position.x=newX;
			bombers[bomber_index].position.y=newY;

			maze[bombers[bomber_index].position.y][bombers[bomber_index].position.x]+=BOMBER_PLACE;

			send_msg.data.new_position.x=newX;
			send_msg.data.new_position.y=newY;
		}
	}
	return send_msg;
}
om bomberPlantOm(int bomber_index,int radius,long interval){

	om send_msg;
	send_msg.type=BOMBER_PLANT_RESULT;
	if(maze[bombers[bomber_index].position.y][bombers[bomber_index].position.x]<=BOMB_PLACE) /*there is bomb*/
	{
		send_msg.data.planted=false;
		return send_msg;
	}
	
	send_msg.data.planted=true;
	total_bomb_count++;
	if(bombs==NULL)
	{
		bombs=malloc(sizeof(Bomb));
	}
	else{
		bombs=realloc(bombs,sizeof(Bomb)*total_bomb_count);
	}
	int index=total_bomb_count-1;
	bombs[index].radius=radius;
	bombs[index].interval=interval;
	bombs[index].position.x=bombers[bomber_index].position.x;
	bombs[index].position.y=bombers[bomber_index].position.y;
	bombs[index].is_exploded=false;
	
	maze[bombs[index].position.y][bombs[index].position.x]+=BOMB_PLACE;

	forkBomb(index);
	return send_msg;
}
om bomberSeeOm(int bomber_index,od *objects){

	om send_msg;
	send_msg.type=BOMBER_VISION;
	int x=bombers[bomber_index].position.x;
	int y=bombers[bomber_index].position.y;
	int obj_cnt=0;
	
	for(int i=x+1; i<x+4 && i < maze_width;i++)
	{
		if(maze[y][i]>0 || maze[y][i]==-1) /*obstacle*/
		{
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=OBSTACLE;
			obj_cnt+=1;
			break;
		}
		else if(maze[y][i]==BOMBER_PLACE) /*bomber*/
		{
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=BOMBER;
			obj_cnt+=1;
		}
		else if(maze[y][i]==BOMB_PLACE) /*bomb*/
		{	
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=BOMB;
			obj_cnt+=1;
		}
		else if(maze[y][i]==(BOMB_PLACE+BOMBER_PLACE)) /*bomber + bomb*/
		{
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=BOMBER;

			objects[obj_cnt+1].position.x=i;
			objects[obj_cnt+1].position.y=y;
			objects[obj_cnt+1].type=BOMB;	
			obj_cnt+=2;
		}
	}
	
	for(int i=x-1; i>x-4 && i >=0 ;i--)
	{
		if(maze[y][i]>0 || maze[y][i]==-1)
		{
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=OBSTACLE;
			obj_cnt+=1;
			break;
		}
		else if(maze[y][i]==BOMBER_PLACE) /*bomber*/
		{
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=BOMBER;
			obj_cnt+=1;
		}
		else if(maze[y][i]==BOMB_PLACE) /*bomb*/
		{	
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=BOMB;
			obj_cnt+=1;
		}
		else if(maze[y][i]==(BOMB_PLACE+BOMBER_PLACE)) /*bomber + bomb*/
		{
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=BOMBER;

			objects[obj_cnt+1].position.x=i;
			objects[obj_cnt+1].position.y=y;
			objects[obj_cnt+1].type=BOMB;	
			obj_cnt+=2;
		}
	}
	
	for(int i=y+1; i<y+4 && i < maze_height;i++)
	{
		if(maze[i][x]>0 || maze[i][x]==-1)
		{
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=OBSTACLE;
			obj_cnt+=1;
			break;
		}
		else if(maze[i][x]==BOMBER_PLACE) /*bomber*/
		{
			objects[obj_cnt].position.x=x;
			objects[obj_cnt].position.y=i;
			objects[obj_cnt].type=BOMBER;
			obj_cnt+=1;
		}
		else if(maze[i][x]==BOMB_PLACE) /*bomb*/
		{
			objects[obj_cnt].position.x=x;
			objects[obj_cnt].position.y=i;
			objects[obj_cnt].type=BOMB;
			obj_cnt+=1;
		}
		else if(maze[i][x]==(BOMB_PLACE+BOMBER_PLACE)) /*bomber + bomb*/
		{
			objects[obj_cnt].position.x=x;
			objects[obj_cnt].position.y=i;
			objects[obj_cnt].type=BOMBER;

			objects[obj_cnt+1].position.x=x;
			objects[obj_cnt+1].position.y=i;
			objects[obj_cnt+1].type=BOMB;
			obj_cnt+=2;
		}
	}

	for(int i=y-1; i>y-4 && i >=0 ;i--)
	{
		
		if(maze[i][x]>0 || maze[i][x]==-1)
		{
			
			objects[obj_cnt].position.x=i;
			objects[obj_cnt].position.y=y;
			objects[obj_cnt].type=OBSTACLE;
			obj_cnt+=1;
			break;
		}
		else if(maze[i][x]==BOMBER_PLACE) /*bomber*/
		{
			
			objects[obj_cnt].position.x=x;
			objects[obj_cnt].position.y=i;
			objects[obj_cnt].type=BOMBER;
			obj_cnt+=1;
		}
		else if(maze[i][x]==BOMB_PLACE) /*bomb*/
		{
			
			objects[obj_cnt].position.x=x;
			objects[obj_cnt].position.y=i;
			objects[obj_cnt].type=BOMB;
			obj_cnt+=1;
		}
		else if(maze[i][x]==(BOMB_PLACE+BOMBER_PLACE)) /*bomber + bomb*/
		{
			
			objects[obj_cnt].position.x=x;
			objects[obj_cnt].position.y=i;
			objects[obj_cnt].type=BOMBER;

			objects[obj_cnt+1].position.x=x;
			objects[obj_cnt+1].position.y=i;
			objects[obj_cnt+1].type=BOMB;
			obj_cnt+=2;
		}
	}
	
	if(maze[y][x]==(BOMB_PLACE+BOMBER_PLACE))
	{
		objects[obj_cnt].position.x=x;
		objects[obj_cnt].position.y=y;
		objects[obj_cnt].type=BOMB;
		obj_cnt+=1;
	}
	
	send_msg.data.object_count=obj_cnt;
	return send_msg;
}
om bomberDieOm(int bomber_index)
{
	om send_msg;
	send_msg.type=BOMBER_DIE;
	bombers[bomber_index].is_died_this_turn=false;
	bombers[bomber_index].is_alive=false;

	maze[bombers[bomber_index].position.y][bombers[bomber_index].position.x]-=BOMBER_PLACE;	
	
	return send_msg;
}

/******REQUESTS********/
void serveRequestBomber(im *msg,int bomber_index)
{
	omp out;
	om send_msg;
	od objects[25];
	out.pid=bombers[bomber_index].pid;
	out.m=&send_msg;
	if(winner==&bombers[bomber_index])
	{	
		send_msg.type=BOMBER_WIN;
		send_message(bombers[bomber_index].file_no,&send_msg);
		print_output(NULL,&out,NULL,NULL);
		bombers[bomber_index].is_alive=false;
		reap(bombers[bomber_index].pid,REAP_BOMBER_MSG);
		informed_bomber+=1;
		return;
	}
	else if(bombers[bomber_index].is_died_this_turn) /*inform the bomber*/
	{
		send_msg=bomberDieOm(bomber_index);
		send_message(bombers[bomber_index].file_no,&send_msg);
		print_output(NULL,&out,NULL,NULL);
		reap(bombers[bomber_index].pid,REAP_BOMBER_MSG);
		informed_bomber+=1;
		return;
	}
	switch (msg->type) {
            case BOMBER_START:
				send_msg=bomberStartOm(bomber_index);
				send_message(bombers[bomber_index].file_no,&send_msg);
				print_output(NULL,&out,NULL,NULL);

				break;
            case BOMBER_MOVE:
				send_msg=bomberMoveOm(bomber_index,msg->data.target_position.x,msg->data.target_position.y);
				send_message(bombers[bomber_index].file_no,&send_msg);
				print_output(NULL,&out,NULL,NULL);
				break;
			case BOMBER_PLANT:
                
					send_msg=bomberPlantOm(bomber_index,msg->data.bomb_info.radius,msg->data.bomb_info.interval);
					send_message(bombers[bomber_index].file_no,&send_msg);
					print_output(NULL,&out,NULL,NULL);
				break;
            case BOMBER_SEE:
				send_msg=bomberSeeOm(bomber_index,objects);
				send_message(bombers[bomber_index].file_no,&send_msg);
				send_object_data(bombers[bomber_index].file_no,send_msg.data.object_count,objects);
				print_output(NULL,&out,NULL,objects);
                break;
        }
}

/*
	Checks coord for explosion
	if  a bomber dies ->  bombers[j].is_died_this_turn=true;	alive_bombers-=1;
	if alive_bombers==1 -> find winner -> winner=&bombers[i];
*/
void check_position_for_exp(int x,int y, bool *is_blocked)
{

	if(maze[y][x]>0 || maze[y][x]==-1) /*obstacle with durability*/
	{
		if(maze[y][x]>0) maze[y][x] -= 1;
		
		obsd obstacle;
		obstacle.position.x=x;
		obstacle.position.y=y;
		obstacle.remaining_durability=maze[y][x];
		*is_blocked=true;
		
		print_output(NULL,NULL,&obstacle,NULL);
		
	}
	else if(maze[y][x]==(BOMBER_PLACE) || maze[y][x]==(BOMB_PLACE+BOMBER_PLACE)) /*bomber position*/
	{
		for(int j=0;j<bomber_count;j++)
		{
			if((bombers[j].position.x==x) && (bombers[j].position.y==y) && winner!=&bombers[j]) /*die*/
			{
				bombers[j].is_died_this_turn=true;
				alive_bombers-=1;
				break;
			}
		}
		if(alive_bombers==1)
		{
			for(int i=0;i<bomber_count;i++)
			{
				if(bombers[i].is_alive==true && bombers[i].is_died_this_turn!=true)
				{
					winner=&bombers[i];
				}
			}
		}
	}
}

void explode(im *msg,int bomb_index) 
{	
	int radius=bombs[bomb_index].interval;
	int x=bombs[bomb_index].position.x;
	int y=bombs[bomb_index].position.y;
	bombs[bomb_index].is_exploded=true;

	maze[y][x]-=BOMB_PLACE;

	check_position_for_exp(x,y,NULL);

	bool is_left_blocked=false,is_right_blocked=false,is_up_blocked=false,is_down_blocked=false;
	for (int i=1;i<=radius;i++)
	{
		int left=x-i,right=x+i,up=y-i,down=y+i;
		if(left<0) is_left_blocked=true;
		if(right>=maze_width) is_right_blocked=true;
		if(up<0) is_up_blocked=true;
		if(down>=maze_height) is_down_blocked=true;

		if(!is_left_blocked)
		{
			check_position_for_exp(left,y,&is_left_blocked);
		}
		if(!is_right_blocked)
		{
			check_position_for_exp(right,y,&is_right_blocked);
		}
		if(!is_up_blocked)
		{
			check_position_for_exp(x,up,&is_up_blocked);
		}
		if(!is_down_blocked)
		{
			check_position_for_exp(x,down,&is_down_blocked);
		}
	}
	reap(bombs[bomb_index].pid,REAP_BOMB_MSG);
}

void inputs()
{
	
	scanf("%d %d %d %d",&maze_width,&maze_height,&obstacle_count,&bomber_count);
    maze=malloc(sizeof(int*)*maze_height);
	bombers=malloc(sizeof(Bomber)*bomber_count);

	for(int i=0;i<maze_height;i++)
	{
		maze[i]=malloc(sizeof(int)*maze_width);
		for(int j=0;j<maze_width;j++)
		{
			maze[i][j]=0;
		}
	}
    for(int i=0;i<obstacle_count;i++)
    {
        int x,y,durability;
        scanf("%d %d %d",&x,&y,&durability);
        maze[y][x]=durability;
    }
	
	
	for(int i=0;i<bomber_count;i++)
	{
		int x,y,total_arg_count;
		char *exec_path;
		char **arg_list;
		scanf("%d %d %d",&x,&y,&total_arg_count);
		arg_list=malloc(sizeof(char*)*total_arg_count);
		for(int j=0;j<total_arg_count;j++)
		{
			scanf("%ms",&arg_list[j]);
		}
		bombers[i].is_alive=true;
		bombers[i].is_died_this_turn=false;
		bombers[i].position.x=x;
		bombers[i].position.y=y;
		maze[y][x]=BOMBER_PLACE;
		forkBomber(i,arg_list);
	}
}

int main()
{
    inputs();

	alive_bombers=bomber_count;
	if(bomber_count==1) winner = &bombers[0];
	while(alive_bombers>1 || informed_bomber!=bomber_count)
	{
		int ready_bomb_count=0;
	
		if(total_bomb_count > 0)
		{
			ready_bomb_count = poll(fds_bombs,total_bomb_count,0);
		}
	
		for(int i=0;(i<total_bomb_count) && (ready_bomb_count>0);i++)
		{
			if((fds_bombs[i].revents & POLLIN) && bombs[i].is_exploded==false)
			{
				im msg; /*incoming_message_data*/
				read_data(bombs[i].file_no,&msg);
				imp in;
				in.m=&msg;
				in.pid=bombs[i].pid;
				
				if(msg.type==BOMB_EXPLODE)
				{
					print_output(&in, NULL, NULL, NULL);
					
					explode(&msg,i);		
				}
				
				ready_bomb_count--;
			}
		}
		
		int ready_bomber_count;
		if((ready_bomber_count = poll(fds_bombers,bomber_count,0)) == -1)
		{
			// perror("poll bomber error");
		}
		for(int i=0;(i<bomber_count) && (ready_bomber_count>0);i++)
		{
			
			if((fds_bombers[i].revents & POLLIN) && bombers[i].is_alive == true)
			{
				
				im msg; /*incoming_message_data*/
				read_data(bombers[i].file_no,&msg);
				imp in;
				in.m=&msg;
				in.pid=bombers[i].pid;
				print_output(&in, NULL, NULL, NULL);
				serveRequestBomber(&msg,i);
				ready_bomber_count--;
			}
		}
		sleep(0.001);
	}
	
	/*Reap Bombs*/
	int active_bomb_count=0;
	for(int i=0;i<total_bomb_count;i++)
	{
		if(bombs[i].is_exploded==false)
		{
			active_bomb_count++;
		}
	}
	while (active_bomb_count!=0)
	{
		int ready_bomb_count=0;
		ready_bomb_count=poll(fds_bombs,total_bomb_count,0);
		for(int i=0;(i<total_bomb_count) && (ready_bomb_count>0);i++)
		{
			if((fds_bombs[i].revents & POLLIN) && bombs[i].is_exploded==false)
			{
				im msg; /*incoming_message_data*/
				read_data(bombs[i].file_no,&msg);
				imp in;
				in.m=&msg;
				in.pid=bombs[i].pid;
				
				if(msg.type==BOMB_EXPLODE)
				{
					print_output(&in, NULL, NULL, NULL);
					
					explode(&msg,i);
				}
				ready_bomb_count--;
				active_bomb_count--;
			}
		}
		sleep(0.001);
	}
    return 0;
}