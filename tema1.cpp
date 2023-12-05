#include <iostream>
#include <pthread.h>
#include <queue>
#include <set>
#include <vector>
#include <math.h>
#include <fstream>

using namespace std;

//clasa abstracta
//care supraincarca operatorul de apel
class Task{

    protected:
        int id;
    public:
        virtual void operator() () = 0;
        void setId(int id){
            this->id = id;
        }
        int getId(){
            return id;
        }
};


// clasa care generalizeaza paradigma map-reduce
class MapReduce{
    private:
        int mappers;
        int reducers;

        //se folosesc cozi de task-uri
        queue<Task*> mapperTasks;
        queue<Task*> reducerTasks;

        pthread_mutex_t mapperMutex;
        pthread_mutex_t reducerMutex;

        //se foloseste o bariera pentru a asigura
        //ca in urma finisarii thread-urilor care se ocupa
        //de mapping vor incepe task-urile de tip reduce
        pthread_barrier_t barrier;
    public:
        MapReduce(int mappers, int reducers, queue<Task*> mapperTasks, queue<Task*> reducerTasks): 
            mappers(mappers), 
            reducers(reducers),
            mapperTasks(mapperTasks),
            reducerTasks(reducerTasks) {
                pthread_mutex_init(&mapperMutex, nullptr);
                pthread_mutex_init(&reducerMutex, nullptr);
                pthread_barrier_init(&barrier, NULL, mappers + reducers);
            }

        void *runMap(int id){
            while(true){

                Task *task;
                bool queueIsEmpty = false;

                //se extrage task-ul din coada
                pthread_mutex_lock(&mapperMutex);
                if(!mapperTasks.empty()){
                    task = mapperTasks.front();
                    mapperTasks.pop();
                }else{
                    queueIsEmpty = true;
                }
                pthread_mutex_unlock(&mapperMutex);

                if(queueIsEmpty)
                    break;

                //se executa task-ul
                task->setId(id);
                (*task)();
            }
            
            //se anunta terminarea task-ului
            pthread_barrier_wait(&barrier);
        }
        void *runReduce(int id){
            
            //se verifica terminarea task-urilor de tip reduce
            pthread_barrier_wait(&barrier);
            while(true){

                Task *task;
                bool queueIsEmpty = false;

                //se extrage task-ul din coada
                pthread_mutex_lock(&reducerMutex);
                if(!reducerTasks.empty()){
                    task = reducerTasks.front();
                    reducerTasks.pop();
                }else{
                    queueIsEmpty = true;
                }
                pthread_mutex_unlock(&reducerMutex);

                if(queueIsEmpty)
                    break;

                //se executa task-ul
                task->setId(id);
                (*task)();
            }
        }

        struct helperStruct{
            MapReduce *object;
            int id;
        };

        //functii ajutatoare pentru a lansa
        //functiile "runMap" si "runReduce"

        static void *helper_run_mapper(void *object){
            helperStruct *str = (helperStruct*) object;
            return str->object->runMap(str->id);
        }
        static void *helper_run_reducer(void *object){
            helperStruct *str = (helperStruct*) object;
            return str->object->runReduce(str->id);
        }

        void start(){

            //se lanseaza toate thread-urile
            pthread_t threads[mappers + reducers];
            for(int i = 0; i < mappers; i++){
                helperStruct *str = new helperStruct{this, i};
                pthread_create(&threads[i], NULL, MapReduce::helper_run_mapper, str);
            }
            for(int i = 0; i < reducers; i++){
                helperStruct *str = new helperStruct{this, i};
                pthread_create(&threads[mappers + i], NULL, MapReduce::helper_run_reducer, str);
            }

            for(int i = 0; i < mappers + reducers; i++){
                void *status;
                pthread_join(threads[i], &status);
            }
        }
};

//clasa de tip Task
//care se ocupa de partea de "mapping" a temei
class MapTask : public Task{
    private:

        //informatiile necesare pentru executarea task-ului
        string fileName;
        vector<vector<vector<int>>> *mapResult;
        int maxPower;
    public:
        MapTask(string fileName, int maxPower, vector<vector<vector<int>>> *mapResult):
            fileName(fileName), 
            maxPower(maxPower), 
            mapResult(mapResult){}

        size_t myPow(size_t number, int exponent){
            size_t result = 1;
            for(int i = 0; i < exponent; i++){
                result *= number;
            }
            return result;
        }

        bool checkPower(int number, int exponent){
            
            if(number == 0)
                return false;
            if(number == 1)
                return true;
            size_t left = 1;
            size_t right = 10000000;

            if(exponent > 2){
                right = 10000;
            }

            while(left < right){
                size_t middle = (left + right + 1) / 2;
                size_t power = myPow(middle, exponent); 

                if(power > number){
                    right = middle - 1;
                }
                if(power < number){
                    left = middle + 1;
                }
                if(power == number){
                    return true;
                }
            }

            if(number == pow(left, exponent) || number == pow(right, exponent))
                return true; 

            return false;
        }

        void operator() () {
            ifstream fin(fileName);
            int currentNumber;
            int numbers;
            fin >> numbers;
            for(int i = 0; i < numbers; i++){
                fin >> currentNumber;
                for(int i = 2; i <= maxPower; i++){
                    if(checkPower(currentNumber, i)){
                        (*mapResult)[id][i - 2].push_back(currentNumber);
                    }
                }
            }
        }
};

//clasa de tip Task
//care se ocupa de partea de "reducing" a temei
class ReduceTask : public Task{
    private:
        int power;
        vector<vector<vector<int>>> *mapResult;
        int mappers;
        set<int> finalResult;
    public:
        ReduceTask(int power, int mappers, vector<vector<vector<int>>> *mapResult):
            power(power), 
            mapResult(mapResult),
            mappers(mappers){

            finalResult = set<int>();
        }
        void operator() () {
            for(int i = 0; i < mappers; i++){
                for(int item : (*mapResult)[i][power - 2]){
                    finalResult.insert(item);
                }
            }
        }

        int getPower(){
            return power;
        }
        set<int> getFinalResult(){
            return finalResult;
        }
};


int main(int argc, char *argv[]){

    int mappers = atoi(argv[1]);
    int reducers = atoi(argv[2]);
    string fileName(argv[3]);

    queue<Task*> mappersTasks = queue<Task*>();
    queue<Task*> reducersTasks = queue<Task*>();

    vector<vector<vector<int>>> *mapResult = new vector<vector<vector<int>>>(mappers, vector<vector<int>>(reducers, vector<int>()));

    ifstream fin(fileName);
    int filesNo;
    fin >> filesNo;

    for(int i = 0; i < filesNo; i++){
        string file;
        fin >> file;

        //se instantiaza obiectele de tip MapTask
        Task* task = new MapTask(file, reducers + 1, mapResult);
        mappersTasks.push(task);
    }

    for(int i = 0; i < reducers; i++){

        //se instantiaza obiectele de tip ReduceTask
        Task* task = new ReduceTask(i + 2, mappers, mapResult);
        reducersTasks.push(task);
    }

    MapReduce mapReduce(mappers, reducers, mappersTasks, reducersTasks);

    //se porneste rezolvarea problemei
    //folosind paradigma map-reduce
    mapReduce.start();

    while(!reducersTasks.empty()){

        ReduceTask* task = (ReduceTask*)reducersTasks.front();
        reducersTasks.pop();

        string outFile = string("out") + to_string(task->getPower()) + string(".txt");
        ofstream fout(outFile);

        fout << task->getFinalResult().size();

        fout.close();
    }

    return 0;
}
