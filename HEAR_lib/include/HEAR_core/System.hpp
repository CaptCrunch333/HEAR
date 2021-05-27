#ifndef SYSTEM_HPP
#define SYSTEM_HPP

#include "HEAR_core/Block.hpp"
#include "HEAR_core/Port.hpp"
#include "HEAR_core/ExternalPort.hpp"
#include "HEAR_core/Graph.hpp"
#include "HEAR_core/DataTypes.hpp"

#include <string>
#include <vector>
#include <algorithm>
#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <atomic>

/**
 * We can use graph concept for the blocks connectivity 
 * 
 **/

namespace HEAR{

class System{
public:
    System(const int frequency) : _dt(1.f/frequency), _exit_flag(false) {}
    ~System(){}
    void printSystem();
    int init();
    void execute();
    void mainLoop();
    void terminate() { _exit_flag = true;}
    int addBlock(Block* blk, std::string name);
    template <class T> int createExternalOutputPort(TYPE dtype, std::string port_name);
    template <class T> int createExternalInputPort(TYPE dtype, std::string port_name);
    template <class T> ExternalOutputPort<T>* getExternalOutputPort(int ext_op_idx);
    template <class T> ExternalInputPort<T>* getExternalInputPort(int ext_ip_idx);
    template <class T> void connectToExternalInput(int ext_ip_idx, int dest_block_uid, int ip_idx);
    template <class T> void connectToExternalOutput(int src_block_uid, int op_idx, int ext_op_idx);
    template <class T> void connect(int out_block_uid, int op, int in_block_uid, int ip);
private:
    int num_blocks = 0;
    float _dt;
    std::atomic<bool> _exit_flag;
    std::vector<Edge> _edges;
    Graph _graph;
    std::vector<Block*> _external_triggers;
    std::vector<Block*> _blocks;
    std::vector<std::string> _block_names;
    std::vector<Block*> seq;
    std::unique_ptr<std::thread> system_thread;
    static bool sortbyconnectivity(const Block* a, const Block* b);
    void findsequence();

};

int System::init(){
    // do some checks for errors in connectivity etc
    this->findsequence();
    _graph = Graph(_edges, _blocks.size());
    return true;
}

template <class T>
int System::createExternalOutputPort(TYPE dtype, std::string port_name){
    ExternalOutputPort<T>* ext_port = new ExternalOutputPort<T>(dtype);
    return this->addBlock(ext_port, port_name);
}
template <class T>
int System::createExternalInputPort(TYPE dtype, std::string port_name){
    ExternalInputPort<T>* ext_port = new ExternalInputPort<T>(dtype);
    return this->addBlock(ext_port, port_name);
}

template <class T> 
ExternalOutputPort<T>* System::getExternalOutputPort(int ext_op_idx){
    return (ExternalOutputPort<T>*)_blocks[ext_op_idx];
}

template <class T> 
ExternalInputPort<T>* System::getExternalInputPort(int ext_ip_idx){
    return (ExternalInputPort<T>*)_blocks[ext_ip_idx];
}

int System::addBlock(Block* blk, std::string name){
    blk->_block_uid = num_blocks;
    this->_blocks.push_back(blk);
    this->_block_names.push_back(name);
    num_blocks++;
    return blk->_block_uid;
}
template <class T>
void System::connect(int out_block_uid, int op, int in_block_uid, int ip){
   ((InputPort<T>*) _blocks[in_block_uid]->getInputPort<T>(ip))->connect(_blocks[out_block_uid]->getOutputPort<T>(op));
   Edge ed;
   ed.src_block_idx = out_block_uid;
   ed.src_port = op;
   ed.dest_block_idx = in_block_uid;
   ed.dest_port = ip;
   _edges.push_back(ed);
}

template <class T>
void System::connectToExternalInput(int ext_ip_idx, int dest_block_uid, int ip_idx){
    this->connect<T>(ext_ip_idx, 0, dest_block_uid, ip_idx);
}

template <class T>
void System::connectToExternalOutput(int src_block_uid, int op_idx, int ext_op_idx){
    this->connect<T>(src_block_uid, op_idx, ext_op_idx, 0);
}


bool System::sortbyconnectivity(const Block* a, const Block* b)
{
    for(auto const &iport : b->getInputPorts()){
        if( iport.second->getConnectedBlockUID() == a->_block_uid){
            return true;
        }
    }
    return false;
}
void System::findsequence(){
    seq = _blocks;
    std::sort(seq.begin(), seq.end(), System::sortbyconnectivity);
}

void System::mainLoop(){
    auto start = std::chrono::steady_clock::now();
    auto end = std::chrono::steady_clock::now();
    auto step_time = std::chrono::microseconds((int)(_dt*1e6));
    std::chrono::duration<double> avglooptime;
    int i = 0;
    while(!_exit_flag){
        start = std::chrono::steady_clock::now();
        // call external triggers   
        for(auto &it : seq){
            it->process();
        }
        end = std::chrono::steady_clock::now();
        auto loop_time = std::chrono::duration_cast<std::chrono::duration<double>>(end - start);
        if(loop_time > step_time){
            // print warning
        }
        else{
            std::this_thread::sleep_for(step_time -loop_time);
        }
        if(i == 100){
            i = 0;
            std::cout << "Loop Frequency : " << 1.0/avglooptime.count() << std::endl;    
            avglooptime = std::chrono::duration<double>(0);
        }   
        avglooptime += (loop_time/100);

    }
}

void System::execute(){
    system_thread = std::unique_ptr<std::thread>(
                    new std::thread(&System::mainLoop, this));
    sleep(3);
}

void System::printSystem(){
    std::cout <<seq.size()<<std::endl;
    for (int i = 0; i<seq.size(); i++){
        int src_blk_idx = seq[i]->_block_uid;
        std::cout<< _block_names[src_blk_idx] <<std::endl;
        auto connections = _graph.adjList[src_blk_idx];
        for(auto const &connection : connections){
            std::cout << _block_names[src_blk_idx] << " | " << seq[i]->getOutputPortName(connection.src_port) << " -----> " 
                        << _blocks[connection.dest_block_idx]->getInputPortName(connection.dest_port)  << " | " << _block_names[connection.dest_block_idx] << std::endl;
        }
    }
}

}
#endif