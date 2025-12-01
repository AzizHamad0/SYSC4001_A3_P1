/**
 * @file interrupts_student1_student2_EP_RR.cpp
 * Combined External Priority + Round Robin (100ms) scheduler
 */

#include <map>
#include <sstream>
#include "interrupts_101232108_101284947.hpp"


void FCFS(std::vector<PCB> &ready_queue) {
    std::sort(
        ready_queue.begin(),
        ready_queue.end(),
        [](const PCB &first, const PCB &second) {
            if (first.PID == second.PID) {
                return (first.arrival_time > second.arrival_time);
            }
            return (first.PID > second.PID);
        }
    );
}

std::string print_memory_status(unsigned int current_time,
                                const std::vector<PCB> &job_list) {
    std::stringstream buffer;

    unsigned int total_partition_memory = 0;
    for (const auto &part : memory_paritions) {
        total_partition_memory += part.size;
    }

    unsigned int total_used_by_processes = 0;
    unsigned int total_usable_free       = 0; 

    buffer << "\n memory status at time " << current_time << " \n";

    for (const auto &part : memory_paritions) {
        buffer << "Partition " << part.partition_number
               << " (" << part.size << "Mb): ";

        if (part.occupied == -1) {
            buffer << "FREE\n";
            total_usable_free += part.size;
        } else {
            unsigned int proc_size = 0;
            for (const auto &p : job_list) {
                if (p.PID == part.occupied) {
                    proc_size = p.size;
                    break;
                }
            }
            total_used_by_processes += proc_size;
            buffer << "PID " << part.occupied << " using "
                   << proc_size << "Mb\n";
        }
    }

    unsigned int total_free_memory = 0;
    if (total_partition_memory >= total_used_by_processes) {
        total_free_memory = total_partition_memory - total_used_by_processes;
    }

    buffer << "memory used: "  << total_used_by_processes << "Mb\n";
    buffer << "free memory: "  << total_free_memory       << "Mb\n";
    buffer << "free partitions: "
           << total_usable_free << "Mb\n";
    buffer << "-------------------\n\n";

    return buffer.str();
}

std::tuple<std::string>
run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;

    struct WaitingEntry {
        PCB          process;
        unsigned int wake_time;   
    };
    std::vector<WaitingEntry> wait_queue;

    std::vector<PCB> job_list = list_processes;

    unsigned int current_time = 0;
    PCB running;
    idle_CPU(running);   

    std::map<int, unsigned int> cpu_since_last_io;

    const unsigned int TIME_QUANTUM = 100;
    unsigned int quantum_counter = 0;

    std::string execution_status;
    execution_status = print_exec_header();

    while (!job_list.empty() && !all_process_terminated(job_list)) {
        for (auto &proc : job_list) {
            if ((proc.state == NOT_ASSIGNED || proc.state == NEW)
                && proc.arrival_time <= current_time) {

                if (assign_memory(proc)) {
                    states old_state =
                        (proc.state == NOT_ASSIGNED ? NEW : proc.state);
                    proc.state = READY;
                    ready_queue.push_back(proc);

                    execution_status += print_exec_status(
                        current_time, proc.PID, old_state, READY
                    );

                    execution_status += print_memory_status(current_time, job_list);
                }
            }
        }

        for (auto it = wait_queue.begin(); it != wait_queue.end();) {
            if (it->wake_time <= current_time) {
                PCB &p = it->process;
                states old_state = p.state;      
                p.state = READY;

                ready_queue.push_back(p);
                sync_queue(job_list, p);

                execution_status += print_exec_status(
                    current_time, p.PID, old_state, READY
                );

                it = wait_queue.erase(it);
            } else {
                ++it;
            }
        }


        if (running.state == NOT_ASSIGNED && !ready_queue.empty()) {
            FCFS(ready_queue);  
            run_process(running, job_list, ready_queue, current_time);

            execution_status += print_exec_status(
                current_time, running.PID, READY, RUNNING
            );

            quantum_counter = 0;

            if (cpu_since_last_io.find(running.PID) == cpu_since_last_io.end()) {
                cpu_since_last_io[running.PID] = 0;
            }
        }

        if (running.state == RUNNING) {
            
            if (running.remaining_time > 0) {
                running.remaining_time -= 1;
            }
            cpu_since_last_io[running.PID] += 1;
            quantum_counter += 1;

            sync_queue(job_list, running);


            unsigned int event_time = current_time + 1;

            if (running.remaining_time == 0) {
                states old_state = RUNNING;
                int pid = running.PID;

                terminate_process(running, job_list);
                execution_status += print_exec_status(
                    event_time, pid, old_state, TERMINATED
                );

                idle_CPU(running);
                cpu_since_last_io.erase(pid);
            }

            else if (running.io_freq > 0 &&
                     cpu_since_last_io[running.PID] >= running.io_freq) {
                states old_state = RUNNING;
                running.state = WAITING;
                cpu_since_last_io[running.PID] = 0;
                WaitingEntry entry;
                entry.process   = running;
                entry.wake_time = event_time + running.io_duration;
                wait_queue.push_back(entry);
                sync_queue(job_list, running);
                execution_status += print_exec_status(
                    event_time, running.PID, old_state, WAITING
                );

                idle_CPU(running);
            }

            else {
                bool higher_prio_ready = false;
                for (const auto &p : ready_queue) {
                    if (p.PID < running.PID) { 
                        higher_prio_ready = true;
                        break;
                    }
                }

                if (higher_prio_ready || quantum_counter >= TIME_QUANTUM) {
                    states old_state = RUNNING;
                    running.state    = READY;

                    ready_queue.push_back(running);
                    sync_queue(job_list, running);

                    execution_status += print_exec_status(
                        event_time, running.PID, old_state, READY
                    );

                    idle_CPU(running);
                }
            }
        }

        current_time += 1;
    }

    execution_status += print_exec_footer();

    return std::make_tuple(execution_status);
}


int main(int argc, char** argv) {

    if(argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received "
                  << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrupts_EP_RR "
                  << "<your_input_file.txt>" << std::endl;
        return -1;
    }

    auto file_name = argv[1];
    std::ifstream input_file(file_name);

    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    std::string line;
    std::vector<PCB> list_process;
    while (std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process  = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    auto [exec] = run_simulation(list_process);
    write_output(exec, "output_files/execution.txt");


    return 0;
}
