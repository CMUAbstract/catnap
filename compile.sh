make bld/$1/depclean SYSTEM=$1 APP=$2 VERBOSE=1
python3 ext/libos/python_src/frontend.py src/$2_catnap.c
make bld/$1/all SYSTEM=$1 APP=$2 VERBOSE=1
echo "SYSTEM: $1, APP: $2"
