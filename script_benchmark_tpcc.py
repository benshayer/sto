import os

print("Start running TPCC script")

NUM_THREADS = [1,4,8,16,24]
for amount_threads in NUM_THREADS:
    print(f"run tpcc with {amount_threads} threads")
    results = []
    for i in range(5):
        print(f"iteration {i}")
        os.system(f"./tpcc_bench -t{amount_threads} >tpcc_result_i{i}_t{amount_threads}.txt")
        with open("tpcc_result.txt") as f:
            lines = f.readlines()
            for line in lines:
                if "Throughput" in line:
                    txns_throughput = float(line.split()[1])
                    results.append(txns_throughput)

    print(f"For {amount_threads} threads, results are: {results}")
