import sys
import fileinput
import re

text = ""

SYS_NAME = "catnap"

def get_undo_input_fifo(fifos):
    result = "void undo_input_fifo()\n{\n"
    for fifo in fifos:
        result += "\tUNDO_FIFO(" + fifo[0] + ", " + fifo[1] + ");\n"
    result += "}\n\n"
    return result

def get_commit_input_fifo(fifos):
    result = "void commit_input_fifo()\n{\n"
    for fifo in fifos:
        result += "\tCOMMIT_FIFO(" + fifo[0] + ", " + fifo[1] + ");\n"
    result += "}\n\n"
    return result

def get_main(periods, params, events_p, events_all, tasks, func_map):
    result = "int main()\n{\n"    

    result += "\tperiod_list_it = 0;\n"
    for period in periods:
        result += "\tperiod_list[period_list_it++] = &" + period + ";\n"

    result += "\tparam_list_it = 0;\n"
    for param in params:
        result += "\tparam_list[param_list_it++] = &" + param + ";\n"

    result += "\tevent_list_all_it = 0;\n"
    for e in events_all:
        result += "\tevent_list_all[event_list_all_it++] = &" + e + ";\n"

    result += "\tevent_list_it = 0;\n"
    for e in events_p:
        result += "\tevent_list[event_list_it++] = &" + e + ";\n"

    result += "\ttask_list_all_it = 0;\n"
    for task in tasks:
        result += "\ttask_list_all[task_list_all_it++] = &" + task + ";\n"

    for name in func_map:
        degradables = func_map[name]
        result += "\t" + name + ".funcs_num = 0;\n"
        for d in degradables:
            result += "\t" + name + ".funcs[" + name + ".funcs_num++] = &" + d + ";\n"

    result += "\tos_main();\n"
    result += "\treturn 0;\n"

    result += "}\n\n"
    return result

def search_and_collect(pattern):
    global text

    result = []
    matches = re.findall(pattern, text)
    for match in matches:
        result.append(match)
    return result

if __name__ == "__main__":
    file_name = sys.argv[1]

    # Read text
    for line in fileinput.input(file_name):
        text += line

    ## Find declarations

    # Find periods
    period_pattern = r"PERIOD\((?P<name>[^,]+),"
    periods = search_and_collect(period_pattern)

    # Find params
    param_pattern = r"PARAM\((?P<name>[^,]+),"
    params = search_and_collect(param_pattern)

    # Periodic events
    event_p_pattern = r"EVENT_PERIODIC\((?P<name>[^,]+),"
    events_p = search_and_collect(event_p_pattern)
    print(events_p)

    # Bursty periodic events
    event_p_pattern = r"EVENT_BURSTY\((?P<name>[^,]+),"
    events_p += search_and_collect(event_p_pattern)
    print(events_p)

    # Aperiodic events
    event_ap_pattern = r"EVENT_APERIODIC\((?P<name>[^,]+),"
    events_all = events_p + search_and_collect(event_ap_pattern)
    print(events_all)

    # Task
    task_pattern = r"TASK\((?P<name>[^,]+),"
    tasks = search_and_collect(task_pattern)
    print(tasks)

    # Funcs
    func_map = {}
    func_pattern = r"FUNCS\((?P<name>[^)]+)\);"
    funcs = search_and_collect(func_pattern)
    for func in funcs:
        fargs = func.split(",")
        for i in range(len(fargs)):
            fargs[i] = fargs[i].strip()
        fname = fargs[0]
        assert(fname not in func_map)
        func_map[fname] = fargs[1:]
    print(func_map)

    # FIFO
    fifo_pattern = r"\nFIFO\((?P<var0>[^,]+), *(?P<var1>[^,]+),"
    fifos = re.findall(fifo_pattern, text)
    print(fifos)

    output_file = file_name.replace(SYS_NAME, "gcc")
    out = open(output_file, "w")

    # Generate the code needed
    out.write(text) 

    # Write undo_input_fifo
    out.write(get_undo_input_fifo(fifos))

    # Write commit_input_fifo
    out.write(get_commit_input_fifo(fifos))

    # Write main
    out.write(get_main(periods, params, events_p, events_all, tasks, func_map))

    out.close()
