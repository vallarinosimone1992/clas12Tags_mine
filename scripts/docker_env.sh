#!/usr/bin/env bash

module load ccdb/1.99.6
module load hipo/4.5.0

export GEMC=/work/clas12Tags_mine
export GEMC_DATA_DIR=/work/clas12Tags_mine

echo "MINE GEMC environment loaded"
echo "  GEMC=$GEMC"
echo "  GEMC_DATA_DIR=$GEMC_DATA_DIR"
echo "  CCDB_HOME=$CCDB_HOME"
echo "  HIPO=$HIPO"
