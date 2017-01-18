#!/bin/bash

RUNS=1

SNAPS=0
FREQUENCY=10
EDGESCANS=1
for i in "$@"
do
case $i in
    -s*|--snaps=*)
    SNAPS="${i#*=}"
    shift
    ;;
    -f*|--freq=*)
    FREQUENCY="${i#*=}"
    shift
    ;;
    -e*|--scans=*)
    EDGESCANS="${i#*=}"
    shift
    ;;
    *)

    ;;
esac
done

# create parentfolder
mkdir -p hh_cm_memes_emes_graphmetrics

# create subfolder
now="$(date +'%d-%m-%Y')"
echo "[combined_ESTFP_graphmetrics] Date = ${now}"
count=$(find ./hh_cm_memes_emes_graphmetrics -type d | wc -l)
echo "[combined_ESTFP_graphmetrics] Folder Count: $((${count}-1))"
echo "[combined_ESTFP_graphmetrics] Next Folder Index: ${count}"
foldername="log${count}_${now}"
mkdir -p hh_cm_memes_emes_graphmetrics/${foldername}
echo "[combined_ESTFP_graphmetrics] Created Folder: hh_cm_memes_emes_graphmetrics/${foldername}"

gamma=(2.0)
mindeg=(10)
nodes=(10000)
divisor=(10)
#gamma=(1.5 2.0)
#mindeg=(5 10)
#nodes=(10000 50000 150000)
#divisor=(10 200)
for g in ${gamma[*]};
do
	for a in ${mindeg[*]};
		do 
			for n in ${nodes[*]};
                do 
                    for div in ${divisor[*]};
                        do
                            for j in `seq 1 $RUNS`;
                            do
                                echo "Doing iteration ${j}"
                                b=$(($n/$div))
                                echo num_nodes $n >> hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.log
                                echo min_deg $a >> hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.log
                                echo max_deg $b >> hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.log
                                echo gamma $g >> hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.log
                                echo divisor $div >> hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.log
                                echo swaps $(($n*10)) >> hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.log 
                                if [ $SNAPS -eq 1 ]
                                then
                                    ./build/memtfp_combined_benchmark -a $a -b $b -g $g -n $n -e TFP -w $EDGESCANS -z -f $FREQUENCY >> hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.log
                                else
                                    ./build/memtfp_combined_benchmark -a $a -b $b -g $g -n $n -e TFP -w $EDGESCANS >> hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.log
                                fi
                                mv ./graph.metis hh_cm_memes_emes_graphmetrics_${a}_${b}_${g}_${div}_${n}_${j}.graphdata
                            done
                            echo "Generating graphmetric file"
                            $(./graph_analyze.sh -f=hh_cm_memes_emes_graphmetrics -a=${a} -b=${b} -g=${g} -d=${div} -n=${n})
                            echo "Removing graphdata file"
                            rm *_${a}_${b}_${g}_${div}_${n}*.graphdata
                       done
                done
        done
done

# move files
mv hh_cm_memes_emes_graphmetrics_*.log hh_cm_memes_emes_graphmetrics/${foldername}
mv sorted_metrics_*.dat hh_cm_memes_emes_graphmetrics/${foldername}
echo "[combined_ESTFP_graphmetrics] Moved Log Files"
