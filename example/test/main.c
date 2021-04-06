#include <mpi.h>
#include <string.h>
#include <stdio.h>
void a(){
    for (int i = 0;i<10;i++){
        int x;
        x++;
    }
}
void b(){
    for(int i = 0;i<100;i++){
        for(int j = 0;j<100;j++){
            a();
        }
        a();
    }
}
void c();
int main(int argc, char** argv){
   char message[20]="";
   int myrank;
   MPI_Status status;
   MPI_Init(&argc,&argv);
   MPI_Comm_rank(MPI_COMM_WORLD,&myrank);
   if(myrank==0)
   {
       strcpy(message,"Hello,process 1");
       MPI_Send(message,strlen(message),MPI_CHAR,1,0,MPI_COMM_WORLD);
   }
   else if(myrank==1)
   {
       MPI_Recv(message,20,MPI_CHAR,0,0,MPI_COMM_WORLD,MPI_STATUS_IGNORE);
       printf("received:%s\n",message);
   }

    for (int i = 0; i<5;i++){
        a();
       if (myrank == 0){
            for(int i = 0;i<100;i++){
                a();
                for(int i = 0;i<100;i++){
                    a();
                }
            }
       }
       printf("Rank:%d already here: MPI_Barrier.\n",myrank);
       MPI_Barrier(MPI_COMM_WORLD);
        int k = 0;
        for(int i = 0;i<100;i++){
            k++;
        }
        b();
        if (myrank == 0){
            c();
        }
    }
   MPI_Finalize();  
}
void c(){
    for(int i = 0;i<100;i++){
        for(int i = 0;i<100;i++){
            a();
        }
        for(int i = 0;i<100;i++){
            b();
        }
    }
}
