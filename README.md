# OrcGC: Automatic Lock-Free Memory Reclamation

OrcGC is a single-header automatic memory reclamation scheme. Think unique\_ptr for lock-free data structures.

The build requires a compiler with C++14 support. If you want to build Natarajan's tree then you'll need C++17 support due to the usage of std::pair. In our experiments both machines were running Ubuntu LTS and using gcc 8.3

	sudo apt-get update
	sudo apt-get install g++-9 gcc-9 python unzip make


## Build the benchmarks

To build the data structures benchmarks go into the graphs folder and type make

	cd graphs/
	make

This will generate the following benchmark executables in the graphs/bin/ folder. 

	bin/q-ll-enq-deq
	bin/set-ll-1k
	bin/set-tree-1m
	bin/set-skiplist-1m


## Run the benchmarks

Still in the graphs/ folder, type './run-all.py'. This will run the relevant benchmarks, saving the results of each in graphs/data/<filename>.txt 

	./run-all.py


## Paper
To appear in PPoPP 2021 "OrcGC: Automatic Lock-Free Memory Reclamation" by Andreia Correia, Pedro Ramalhete and Pascal Felber.