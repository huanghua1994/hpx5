#!/bin/bash -x



OP=$1
DIR=$2
shift

if [ "$HPXMODE_AXIS" == smp ] ;
then
  export NUMNODES=1
elif [ "$SYSTEM" == CREST_cutter ] ;
then
  export NUMNODES=10
else
  export NUMNODES=16
fi

function add_init() {
case "$SYSTEM" in
  CREST_cutter)
    . /opt/modules/Modules/3.2.10/init/bash
    export INPUT_DIR=/u/jsfiroz/DIMACS/ch9-1.1/inputs/Random4-n
    export RUNCMD="mpirun -n $NUMNODES --map-by node:PE=16 --tag-output"
    ;;
  HPX5_BIGRED2 | MARCINS_SWAN)
    module load java
    module unload PrgEnv-cray
    module load PrgEnv-gnu
    module load craype-hugepages8M
    export CRAYPE_LINK_TYPE=dynamic
    export PATH=/N/home/h/p/hpx5/BigRed2/new_modules/bin:$PATH
    export INPUT_DIR=/N/dc2/scratch/zalewski/dimacs/Random4-n
    export RUNCMD="aprun -n $NUMNODES -N 1 -b"
    ;;
  *)
    echo "Unknown system $SYSTEM."
    exit 1
    ;;
esac
case "$SIZE_CHOICE" in
  long)
    export TIMEOUT="60"
    ;;
  *)
    ;;
esac
case "$HPXCORES_AXIS" in
  all)
    ;;
  *)
    export HPXCORES=" --hpx-cores=$HPXCORES_AXIS"
    ;;
esac
case "$BUILD_AXIS" in
  static)
    CFGFLAGS+=" --enable-static --disable-shared"
    ;;
  *)
    CFGFLAGS+=" --disable-static --enable-shared"
    ;;
esac
}

function add_mpi() {
case "$SYSTEM" in
  CREST_cutter)
    module load openmpi/1.8.4_thread
    CFGFLAGS+=" --with-mpi=ompi"
    ;;
  HPX5_BIGRED2 | MARCINS_SWAN)
    CFGFLAGS+=" --with-mpi"
    ;;
esac
}

function add_photon() {
CFGFLAGS+=" --enable-photon"
case "$SYSTEM" in
  CREST_cutter)
    export HPX_PHOTON_IBDEV=$HPXIBDEV
    export HPX_PHOTON_BACKEND=verbs
    # verbs/rdmacm library not in jenkins node config
    export LD_LIBRARY_PATH=/usr/lib64:$LD_LIBRARY_PATH
    export LIBRARY_PATH=/usr/lib64:$LIBRARY_PATH
    ;;
  HPX5_BIGRED2 | MARCINS_SWAN)
    export HPX_PHOTON_BACKEND=ugni
    export HPX_PHOTON_CARGS="--with-ugni"
    CFGFLAGS+=" --with-pmi --with-hugetlbfs"
    ;;
esac
}

function do_build() {
    echo "Building HPX in $DIR"
    cd "$DIR"
    
    echo "Bootstrapping HPX."
    ./bootstrap
    
    if [ -d "./build" ]; then
        rm -rf ./build/
    fi
    mkdir build
    cd build
    
    if [ -d "./install" ]; then
        rm -rf ./install/
    fi
    mkdir install
    
    echo "Configuring HPX."
    $CFG_CMD --prefix="${DIR}/build/install/" ${HPXDEBUG} ${CFGFLAGS}
    
    echo "Building HPX."
    make -j 128
    make install
}

CFGFLAGS=" ${JEMALLOC_AXIS} --enable-apps --enable-parallel-config"

add_init

case "$HPXMODE" in
    photon)
	add_mpi
        add_photon
	;;
    mpi)
	add_mpi	
	;;
    *)
	;;
esac

case "$HPXIBDEV" in
    qib0)
        export PSM_MEMORY=large
	RUNCMD+=" --mca btl_openib_if_include ${HPXIBDEV}"
	;;
    mlx4_0)
	RUNCMD+=" --mca mtl ^psm --mca btl_openib_if_include ${HPXIBDEV}"
	;;
    *)
	;;
esac

case "$SYSTEM" in
  CREST_cutter)
    CFGFLAGS+=" CC=gcc"
    ;;
  HPX5_BIGRED2 | MARCINS_SWAN)
    CFGFLAGS+=" CC=cc"
    ;;
  *)
    exit 1
    ;;
esac

if [ "$OP" == "build" ]; then
    CFG_CMD="../configure"
    do_build
fi

if [ "$OP" == "run" ]; then
    cd "$DIR/build"

    # TBD: fine tune the limits
    # TBD: copy the inputs to the jenkins accounts
        cd apps/libPXGL/examples
        # Delta-Stepping
	$RUNCMD ./sssp -q $TIMEOUT -c -z 40000 $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.22.0.gr $INPUT_DIR/Random4-n.22.0.ss || { echo 'SSSP test failed' ; exit 1; }
        # Chaotic
        $RUNCMD ./sssp -q $TIMEOUT $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.20.0.gr $INPUT_DIR/Random4-n.20.0.ss || { echo 'SSSP test failed' ; exit 1; }
        # Distributed control
        $RUNCMD ./sssp -q $TIMEOUT -d $HPXCORES --hpx-heap=$((1024 * 1024 * 1024 * 3)) --hpx-sendlimit=128 --hpx-transport=$HPXMODE_AXIS --hpx-recvlimit=512 $INPUT_DIR/Random4-n.20.0.gr $INPUT_DIR/Random4-n.20.0.ss || { echo 'SSSP test failed' ; exit 1; }
fi

exit 0
