#include <fstream>
#include <stdio.h>
#include <thread>
#include <mutex>
#include <vector>
#include <iostream>
#include <strings.h>
#include <string>
#include <queue>
#include <algorithm>
#include "mpi.h"
#include "My_barrier.h"
using namespace std;

#define MASTER 0
#define HORROR 1
#define COMEDY 2
#define FANTASY 3
#define SCIFI 4
#define NR_WORKERS 4
#define LINES_PER_THREAD 20

static long long next_para = 0;

static mutex only_read_mutex;

static mutex only_write_mutex;

static mutex send_recv_mutex[NR_WORKERS+1];

static long long position=-2;

static long long current_para=0;

mutex gigel;

auto compare = [](pair<int,string> a,pair<int,string> b) {return a.first>b.first;};

priority_queue<pair<int,string>,vector<pair<int,string>>,decltype(compare)> message_q(compare);

FILE *out;

My_barrier barr(NR_WORKERS);

void process(int id,char *in_file_name)
{
    
    ifstream fin(in_file_name);


    string paragraph;

    int para_number;

    fin.seekg(0,fin.beg);

    char *processed_para;

    int processed_length;

    int send_len;

    int x=0;

    MPI_Status status;

    while (position!=-1)
    {
        //citire
        // aplicam un mutex

        only_read_mutex.lock();

        if(position==-1)
        {
            only_read_mutex.unlock();
            break;
        }

        int consecutive_end=0;  // aceasta variabila numara de cate ori am dat consecutiv peste un terminator '\n' sau EOF

        char c;


        //daca e pentru prima oara cand se face citirea nu se schimba pozitita in fisier
        if(position!=-2)
                fin.seekg(position);


        while (consecutive_end<2)
        {
            
            c=(char)fin.get();
            
            paragraph.push_back(c);

            if(c!='\n' && c!=-1)
                consecutive_end=0;
            else if(c=='\n')
                consecutive_end++;
            else
            {
                consecutive_end=2;
                paragraph.pop_back();
            }
            

        }

        // variabila globala va memora noua pozitie in fisier pentru firele urmatoare
        position=fin.tellg();

        //firul memoreaza al catalea paragraf din fisier a citit
        para_number=next_para;

        // urmatorul fir va lua urmatorul paragraf
        next_para++;

        //firul curent a terminat de citit
        // si elibereaza lacatul
        only_read_mutex.unlock();

        // alege la ce worker va trimite

        int destination;

       

        switch (paragraph[0])
        {
        case 'h':
            destination=HORROR;
            break;
        case 'c':
            destination=COMEDY;
            break;
        case 'f':
            destination=FANTASY;
            break;
        case 's':
            destination=SCIFI;
            break;      
        }

        // lasa doar un singur fir sa trimita catre respectivul worker
        send_recv_mutex[destination].lock();

        send_len=paragraph.length();

        // trimite lungimea paragrafului
        MPI_Send( &send_len , 1 , MPI_INT , destination , 0 , MPI_COMM_WORLD);

        //trimite paragraful
        MPI_Send( paragraph.c_str() , paragraph.length() , MPI_CHAR , destination , 0 , MPI_COMM_WORLD);

        // curata string-ul in care era paragraful neprocesat
        paragraph.clear();

        // primeste mai intai un mesaj cu lungimea paragrafului procesat
        MPI_Recv(&processed_length, 1, MPI_INT, destination, 0, MPI_COMM_WORLD, &status);
        
        //aloca dinamic bufferul in care vom stoca paragraful procesat de worker
        processed_para = new char[processed_length];

        //primeste noul paragraf
        MPI_Recv( processed_para , processed_length , MPI_CHAR , destination , 0 , MPI_COMM_WORLD , &status);

        


        //elibereaza mutexul pus pe worker-ul respectiv
        send_recv_mutex[destination].unlock();

        // doar un singur fir va putea scrie
        only_write_mutex.lock();

        string processed_para_string;

        for(int i=0;i<processed_length;i++)
            processed_para_string.push_back(processed_para[i]);
        
        //cout<<processed_para_string;



        // pune paragraful primit intr-o coada cu prioritati
        // paragrafele sunt ordonate crescator in functie de ordinea de citire a lor
        // informatiile din coada sunt stocate ca pereche<ordine paragraf,paragraf>
        message_q.push(pair<int,string>(para_number,processed_para_string));

    
        
        //scoate din coada doar paragraful din varf care are si id-ul urmatorului paragraf ce trebuie asteptat
        //si repeta procesul pana cand ori coada e goala ori id-ul intalnit nu este cel asteptat (ex:putem scoate paragraful 7 din coada dar inca nu a fost procesat si trimis inapoi paragraful 6)
        while (message_q.empty()==false && message_q.top().first==current_para)
        {

            fprintf(out,"%s",message_q.top().second.c_str());

            // am terminat cu paragraful x , acum paragraful ce urmeaza este x+1
            current_para++;
            message_q.pop();
        }

        // am terminat de scris asa ca eliberam mutexul
        only_write_mutex.unlock();

        // dezalocam memorie
        delete[] processed_para;

        
    }
    
    fin.close();

    // asteptam ca toate cele 4 fire sa isi termine interactiunea cu workerii
    // ele asteapta la o bariera ce lasa exact 4 fire sa intre
    barr.my_wait();

    // fiecare fie va trimite un meseaj ca va fi interpretat de worker ca find lungimea unui paragraf
    // insa lungimea va fi 0
    int terminate=0;
    MPI_Send( &terminate, 1 , MPI_INT , id+1, 0 , MPI_COMM_WORLD);

    



}

char *horror_recv;
vector<string> horror_lines;
void make_horror(int id)
{
    int start = id*LINES_PER_THREAD+1;

    int stop = min((id+1)*LINES_PER_THREAD,(int)horror_lines.size());

    char c;

    for(int i=start;i<stop;i++){
        for(int j=0;j<horror_lines[i].length()-1;j++)
            if(strchr("AEIOUaeiou",horror_lines[i][j])==NULL && ((horror_lines[i][j]>='A' && horror_lines[i][j]<='Z') || (horror_lines[i][j]>='a' && horror_lines[i][j]<='z')))
            {
                if(horror_lines[i][j]>='A' && horror_lines[i][j]<='Z')
                {   
                    c=horror_lines[i][j]+32;
                }
                else
                {
                    c=horror_lines[i][j];
                }

                horror_lines[i].insert(j+1,1,c);

                j++;
               
                
            }
        int n=horror_lines[i].length();
        if(strchr("AEIOUaeiou",horror_lines[i][n-1])==NULL && ((horror_lines[i][n-1]>='A' && horror_lines[i][n-1]<='Z') || (horror_lines[i][n-1]>='a' && horror_lines[i][n-1]<='z')))
        {
            if(horror_lines[i][horror_lines[i].length()-1]>='A' && horror_lines[i][horror_lines[i].length()-1]<='Z')
            c=horror_lines[i][horror_lines[i].length()-1]+32;
            else
            c=horror_lines[i][horror_lines[i].length()-1];

            horror_lines[i].push_back(c);
            
        }    
    }
    
}
char *comedy_recv;
vector<string> comedy_lines;
void make_comedy(int id)
{
    int start = id*LINES_PER_THREAD+1;

    int stop = min((id+1)*LINES_PER_THREAD,(int)comedy_lines.size());

    char c;

    int counter=1;

    for(int i=start;i<stop;i++)
    {
        for(int j=0;j<comedy_lines[i].length();j++)
        {
            if((comedy_lines[i][j]>='A' && comedy_lines[i][j]<='Z') || (comedy_lines[i][j]>='a' && comedy_lines[i][j]<='z'))
            {
                counter=1;
                while (j<comedy_lines[i].length() && ((comedy_lines[i][j]>='A' && comedy_lines[i][j]<='Z') || (comedy_lines[i][j]>='a' && comedy_lines[i][j]<='z')))
                {   
                    if(counter%2==0)
                        comedy_lines[i][j]-=32;
                    
                    j++;
                    counter++;
                }
                
            }
        }
    }

}
char *fantasy_recv;
vector<string> fantasy_lines;
void make_fantasy(int id)
{
    int start = id*LINES_PER_THREAD+1;

    int stop = min((id+1)*LINES_PER_THREAD,(int)fantasy_lines.size());

    for(int i=start;i<stop;i++)
    {
        for(int j=0;j<fantasy_lines[i].length();j++)
        {
            if((fantasy_lines[i][j]>='A' && fantasy_lines[i][j]<='Z') || (fantasy_lines[i][j]>='a' && fantasy_lines[i][j]<='z'))
            {
                if(fantasy_lines[i][j]>='a' && fantasy_lines[i][j]<='z')
                    fantasy_lines[i][j]-=32;
                
                while (j<fantasy_lines[i].length() && ((fantasy_lines[i][j]>='A' && fantasy_lines[i][j]<='Z') || (fantasy_lines[i][j]>='a' && fantasy_lines[i][j]<='z')))
                {
                    j++;
                }
                
            }
            
        }
    }
}

char *scifi_recv;
vector<string> scifi_lines;
void make_scifi(int id)
{
    int start = id*LINES_PER_THREAD+1;

    int stop = min((id+1)*LINES_PER_THREAD,(int)scifi_lines.size());

    int counter=1,k,x;
    
    for(int i=start;i<stop;i++)
    {
        for(int j=0;j<scifi_lines[i].length();j++)
        {
            if((scifi_lines[i][j]>='A' && scifi_lines[i][j]<='Z') || (scifi_lines[i][j]>='a' && scifi_lines[i][j]<='z'))
            {

                if(counter==7)
                {
                    for(k=j;k<scifi_lines[i].length() && ((scifi_lines[i][k]>='A' && scifi_lines[i][k]<='Z') || (scifi_lines[i][k]>='a' && scifi_lines[i][k]<='z'));k++);

                    for(int x=j;x<j+(k-j)/2;x++)
                        swap(scifi_lines[i][x],scifi_lines[i][k-(x-j)-1]);

                    j=k;

                    counter=1;

                }
                else
                {
                    
                    while (j<scifi_lines[i].length() && ((scifi_lines[i][j]>='A' && scifi_lines[i][j]<='Z') || (scifi_lines[i][j]>='a' && scifi_lines[i][j]<='z')))
                    {
                        j++;
                    }
                }

                counter++;
                
            }
        }
    }

}
int main(int argc,char *argv[])
{
    int  numtasks, rank, provided;

    MPI_Init_thread( &argc , &argv , MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);


    // nodul master
    if(rank==MASTER)
    {
        ifstream fin(argv[1]);

        // facem fisierul de iesire 
        char* out_file_name = new char[strlen(argv[1]+1)];

        for(int i=0;i<6;i++)
            out_file_name[i]=argv[1][i];
        out_file_name[6]='o';
        out_file_name[7]='u';
        out_file_name[8]='t';

        for(int i=8;i<strlen(argv[1]);i++)
            out_file_name[i+1]=argv[1][i];

        out=fopen(out_file_name,"a");
        
        
        
        vector<thread> master_threads;
        for(int i=0;i<numtasks-1;i++)
        {
            master_threads.push_back(thread(process,i,argv[1]));
        }

        for(int i=0;i<numtasks-1;i++)
        {
            master_threads[i].join();
        }

        fclose(out);



    }
    else if(rank==HORROR)
    {
        int recv_len,send_len;
        
        MPI_Status status;

        int P;

        char *token;

        unsigned max_local_threads = thread::hardware_concurrency()-1;

        vector<thread> horror_threads;

        string final_horror;

        while(1)
        {
        
            // primeste mesaj cu numarul de caractere ce trebuie sa primeasca
            MPI_Recv( &recv_len , 1 , MPI_INT , MASTER , 0 , MPI_COMM_WORLD , &status);

            //daca a primit un mesaj cu 0 asta inseamna ca procesul MASTER s-a terminat
            // deci workerul trebuie sa iasa din bucla infinita
            if(recv_len==0)
                break;

            horror_recv = new char[recv_len];

            // primeste paragraful
            MPI_Recv( horror_recv , recv_len , MPI_CHAR , MASTER , 0 , MPI_COMM_WORLD , &status);


            // fiecare linie a paragrafului este pusa intr-un vector
            token=strtok(horror_recv,"\n");
            while (token!=NULL)
            {
                horror_lines.push_back(string(token));

                token=strtok(NULL,"\n");
            }

            // aflam cate fire trebuie alocate 
            if((horror_lines.size()-1)%LINES_PER_THREAD==0)
            {
                if((horror_lines.size()-1)/LINES_PER_THREAD<=max_local_threads)
                    P=(horror_lines.size()-1)/LINES_PER_THREAD;
                else
                    P=max_local_threads;
                
            }
            else
            {
                if((horror_lines.size()-1)/LINES_PER_THREAD+1<=max_local_threads)
                    P=(horror_lines.size()-1)/LINES_PER_THREAD+1;
                else
                    P=max_local_threads;
                
            }

            //cream firele
            for(int i=0;i<P;i++)
                horror_threads.push_back(thread(make_horror,i));

            for(int i=0;i<P;i++)
                horror_threads[i].join();

            //refacem noul paragraf prin concatenarea tuturor liniilor
            for(int i=0;i<horror_lines.size();i++)
            {
                final_horror+=horror_lines[i];
                final_horror+="\n";
            }

            final_horror+="\n";

            //cout<<final_horror<<"\n";

            send_len=final_horror.length();

            //trimitem spre Master lungimea paragraflui procesat
            MPI_Send( &send_len , 1 , MPI_INT , MASTER , 0 , MPI_COMM_WORLD);

            //cout<<send_len<<"\n";

            //trimitem paragraful procesat
            MPI_Send( final_horror.c_str() , send_len , MPI_CHAR , MASTER , 0 , MPI_COMM_WORLD);

            //dezalocam mememorie
            final_horror.clear();
            horror_lines.clear();
            horror_threads.clear();
            delete[] horror_recv;
        
        }
    }
    else if(rank==COMEDY)
    {
        int recv_len,send_len;

        MPI_Status status;

        int P;

        char *token;

        unsigned max_local_threads = thread::hardware_concurrency()-1;

        vector<thread> comedy_threads;

        string final_comedy;

        while (1)
        {
            // primeste mesaj cu numarul de caractere ce trebuie sa primeasca
            MPI_Recv( &recv_len , 1 , MPI_INT , MASTER , 0 , MPI_COMM_WORLD , &status);

            //daca a primit un mesaj cu 0 asta inseamna ca procesul MASTER s-a terminat
            // deci workerul trebuie sa iasa din bucla infinita
            if(recv_len==0)
                break;

            comedy_recv = new char[recv_len];

            // primeste paragraful
            MPI_Recv( comedy_recv , recv_len , MPI_CHAR , MASTER , 0 , MPI_COMM_WORLD , &status);

           
            

            // fiecare linie a paragrafului este pusa intr-un vector
            token=strtok(comedy_recv,"\n");
            while (token!=NULL)
            {
                comedy_lines.push_back(string(token));

                token=strtok(NULL,"\n");
            }

            // aflam cate fire trebuie alocate 
            if((comedy_lines.size()-1)%LINES_PER_THREAD==0)
            {
                if((comedy_lines.size()-1)/LINES_PER_THREAD<=max_local_threads)
                    P=(comedy_lines.size()-1)/LINES_PER_THREAD;
                else
                    P=max_local_threads;
                
            }
            else
            {
                if((comedy_lines.size()-1)/LINES_PER_THREAD+1<=max_local_threads)
                    P=(comedy_lines.size()-1)/LINES_PER_THREAD+1;
                else
                    P=max_local_threads;
                
            }

            //cream firele
            for(int i=0;i<P;i++)
                comedy_threads.push_back(thread(make_comedy,i));

            for(int i=0;i<P;i++)
                comedy_threads[i].join();
            
            //refacem noul paragraf prin concatenarea tuturor liniilor
            for(int i=0;i<comedy_lines.size();i++)
            {
                final_comedy+=comedy_lines[i];
                final_comedy+="\n";
            }

            final_comedy+="\n";

            //cout<<final_comedy<<"\n";

            send_len=final_comedy.length();

            //trimitem spre Master lungimea paragraflui procesat
            MPI_Send( &send_len , 1 , MPI_INT , MASTER , 0 , MPI_COMM_WORLD);

            //trimitem paragraful procesat
            MPI_Send( final_comedy.c_str() , send_len , MPI_CHAR , MASTER , 0 , MPI_COMM_WORLD);
            
            //dezalocam memorie
            comedy_threads.clear();
            comedy_lines.clear();
            final_comedy.clear();
            delete[] comedy_recv;


               
        }
        
    }
    else if(rank==FANTASY)
    {
        int recv_len,send_len;

        MPI_Status status;

        int P;

        char *token;

        unsigned max_local_threads = thread::hardware_concurrency()-1;

        vector<thread> fantasy_threads;

        // no pun intended
        string final_fantasy;

        while (1)
        {
            // primeste mesaj cu numarul de caractere ce trebuie sa primeasca
            MPI_Recv( &recv_len , 1 , MPI_INT , MASTER , 0 , MPI_COMM_WORLD , &status);

            //daca a primit un mesaj cu 0 asta inseamna ca procesul MASTER s-a terminat
            // deci workerul trebuie sa iasa din bucla infinita
            if(recv_len==0)
                break;

            fantasy_recv = new char[recv_len];

            // primeste paragraful
            MPI_Recv( fantasy_recv , recv_len , MPI_CHAR , MASTER , 0 , MPI_COMM_WORLD , &status);

            


            // fiecare linie a paragrafului este pusa intr-un vector
            token=strtok(fantasy_recv,"\n");
            while (token!=NULL)
            {
                fantasy_lines.push_back(string(token));

                token=strtok(NULL,"\n");
            }

             // aflam cate fire trebuie alocate 
            if((fantasy_lines.size()-1)%LINES_PER_THREAD==0)
            {
                if((fantasy_lines.size()-1)/LINES_PER_THREAD<=max_local_threads)
                    P=(fantasy_lines.size()-1)/LINES_PER_THREAD;
                else
                    P=max_local_threads;
                
            }
            else
            {
                if((fantasy_lines.size()-1)/LINES_PER_THREAD+1<=max_local_threads)
                    P=(fantasy_lines.size()-1)/LINES_PER_THREAD+1;
                else
                    P=max_local_threads;
                
            }

            for(int i=0;i<P;i++)
                fantasy_threads.push_back(thread(make_fantasy,i));

            for(int i=0;i<P;i++)
                fantasy_threads[i].join();

            //refacem noul paragraf prin concatenarea tuturor liniilor
            for(int i=0;i<fantasy_lines.size();i++)
            {
                final_fantasy+=fantasy_lines[i];
                final_fantasy+="\n";
            }

            final_fantasy+="\n";

            //cout<<final_fantasy<<"\n";

            send_len=final_fantasy.length();

            //trimitem spre Master lungimea paragraflui procesat
            MPI_Send( &send_len , 1 , MPI_INT , MASTER , 0 , MPI_COMM_WORLD);

            //trimitem paragraful procesat
            MPI_Send( final_fantasy.c_str() , send_len , MPI_CHAR , MASTER , 0 , MPI_COMM_WORLD);
            
            //dezalocam memorie
            fantasy_threads.clear();
            fantasy_lines.clear();
            final_fantasy.clear();
            delete[] fantasy_recv;


            
        }
        


    }
    else if(rank==SCIFI)
    {
        int recv_len,send_len;

        MPI_Status status;

        int P;

        char *token;

        unsigned max_local_threads = thread::hardware_concurrency()-1;

        vector<thread> scifi_threads;

        string final_scifi;

        while(1)
        {
            // primeste mesaj cu numarul de caractere ce trebuie sa primeasca
            MPI_Recv( &recv_len , 1 , MPI_INT , MASTER , 0 , MPI_COMM_WORLD , &status);

            //daca a primit un mesaj cu 0 asta inseamna ca procesul MASTER s-a terminat
            // deci workerul trebuie sa iasa din bucla infinita
            if(recv_len==0)
                break;

            scifi_recv = new char[recv_len];

            // primeste paragraful
            MPI_Recv( scifi_recv , recv_len , MPI_CHAR , MASTER , 0 , MPI_COMM_WORLD , &status);


            // fiecare linie a paragrafului este pusa intr-un vector
            token=strtok(scifi_recv,"\n");
            while (token!=NULL)
            {
                scifi_lines.push_back(string(token));

                token=strtok(NULL,"\n");
            }

             // aflam cate fire trebuie alocate 
            if((scifi_lines.size()-1)%LINES_PER_THREAD==0)
            {
                if((scifi_lines.size()-1)/LINES_PER_THREAD<=max_local_threads)
                    P=(scifi_lines.size()-1)/LINES_PER_THREAD;
                else
                    P=max_local_threads;
                
            }
            else
            {
                if((scifi_lines.size()-1)/LINES_PER_THREAD+1<=max_local_threads)
                    P=(scifi_lines.size()-1)/LINES_PER_THREAD+1;
                else
                    P=max_local_threads;
                
            }

            for(int i=0;i<P;i++)
                scifi_threads.push_back(thread(make_scifi,i));

            for(int i=0;i<P;i++)
                scifi_threads[i].join();
            
            //refacem noul paragraf prin concatenarea tuturor liniilor
            for(int i=0;i<scifi_lines.size();i++)
            {
                final_scifi+=scifi_lines[i];
                final_scifi+="\n";
            }

            final_scifi+="\n";

            //cout<<final_scifi<<"\n";

            send_len=final_scifi.length();

            //trimitem spre Master lungimea paragraflui procesat
            MPI_Send( &send_len , 1 , MPI_INT , MASTER , 0 , MPI_COMM_WORLD);

            //trimitem paragraful procesat
            MPI_Send( final_scifi.c_str() , send_len , MPI_CHAR , MASTER , 0 , MPI_COMM_WORLD);
            
            //dezalocam memorie
            scifi_threads.clear();
            scifi_lines.clear();
            final_scifi.clear();
            delete[] scifi_recv;


        }

    }
    

    MPI_Finalize();
}