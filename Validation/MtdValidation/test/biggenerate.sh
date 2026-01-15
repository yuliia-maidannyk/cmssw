#!/bin/bash
NUMBER=$1
SHIFT=15
JOBID=$((NUMBER + SHIFT))
NEVENTS=100
OUTDIR=root://eosuser.cern.ch//eos/user/y/yumaidan/SingleGammaFlatPt8To150/

# ~~~~~~~~~~~~~~~ Setup the environment ~~~~~~~~~~~~~~~ 
cd /afs/cern.ch/user/y/yumaidan/CMSSW_15_1_0_pre1/src
pwd
cmsenv

# ~~~~~~~~~~~~~~~ Run the steps ~~~~~~~~~~~~~~~ 

# Step 1
echo "Running Step 1"
cmsRun step1.py --jobId=${JOBID} --n=${NEVENTS} > step1_${JOBID}.log 2>&1
if [ $? -ne 0 ]; then echo "Step1 failed"; exit 1; fi

# Step 2
echo "Running Step 2"
cmsRun step2.py --jobId=${JOBID} --n=${NEVENTS} > step2_${JOBID}.log 2>&1
if [ $? -ne 0 ]; then echo "Step2 failed"; exit 1; fi

# Remove intermediate files
filename=step1_${JOBID}
#echo "Transferring ${filename} to EOS : ${OUTDIR}"
#xrdcp -f ${filename}.log ${OUTDIR}/logs/${filename}.log
#xrdcp -f ${filename}.root ${OUTDIR}/${filename}.root
#rm -f ${filename}.root
#rm -f ${filename}.log

# Step 3
echo "Running Step 3"
cmsRun step3.py --jobId=${JOBID} --n=${NEVENTS} > step3_${JOBID}.log 2>&1
if [ $? -ne 0 ]; then echo "Step3 failed"; exit 1; fi

# Remove intermediate files
filename=step2_${JOBID}
#echo "Transferring ${filename} to EOS : ${OUTDIR}"
#xrdcp -f ${filename}.log ${OUTDIR}/logs/${filename}.log
#xrdcp -f ${filename}.root ${OUTDIR}/${filename}.root
#rm -f ${filename}.root
#rm -f ${filename}.log

# ~~~~~~~~~~~~~~~ Transfer files to EOS after job completion ~~~~~~~~~~~~~~~ 

#source transfer.sh $JOBID

