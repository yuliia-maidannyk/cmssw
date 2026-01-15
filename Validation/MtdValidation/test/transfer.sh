#!/bin/bash
JOBID=$1
OUTDIR=root://eosuser.cern.ch//eos/user/y/yumaidan/SingleGammaFlatPt8To150/

filename="RandomEngineStates_${JOBID}.txt"
echo "Transferring ${filename} to EOS : ${OUTDIR}"
xrdcp -f ${filename} ${OUTDIR}/logs/${filename}
rm ${filename}

# filename=step1_${JOBID}
# echo "Transferring ${filename} to EOS : ${OUTDIR}"
# xrdcp -f ${filename}.log ${OUTDIR}/${filename}.log
# xrdcp -f ${filename}.root ${OUTDIR}/${filename}.root
# xrdcp -f ${filename}.py ${OUTDIR}/${filename}.py
# rm ${filename}.log
# rm ${filename}.root

# filename=step2_${JOBID}
# echo "Transferring ${filename} to EOS : ${OUTDIR}"
# xrdcp -f ${filename}.log ${OUTDIR}/${filename}.log
# xrdcp -f ${filename}.root ${OUTDIR}/${filename}.root
# xrdcp -f ${filename}.py ${OUTDIR}/${filename}.py
# rm ${filename}.log
# rm ${filename}.root

filename=step3_${JOBID}
echo "Transferring ${filename} to EOS : ${OUTDIR}"
xrdcp -f ${filename}.log ${OUTDIR}/logs/${filename}.log
xrdcp -f ${filename}.root ${OUTDIR}/${filename}.root
# xrdcp -f ${filename}.py ${OUTDIR}/${filename}.py
rm ${filename}.log
# rm ${filename}.root

filename="tree_${JOBID}.root"
echo "Transferring ${filename} to EOS : ${OUTDIR}"
xrdcp -f ${filename} ${OUTDIR}/${filename}
rm ${filename}
