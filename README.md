# spbu-vm-2-lama-bc-interpret

YAILama: Yet Another Interpreter for Lama

## Prerequisites

`lamac` available in PATH

## Usage

After `git clone`, please initialize all submodules:
```
git submodule update --init --recursive
```

`make` to build the interpreter `./YAILama`

`./YAILama <BYTECODE.bc>` to interpret a bytecode file.

`make regression` and `make regression-expressions`

`make performance` On my machine:

```
`which time` -f "Sort\t%U" ./Sort
Sort    1.09
`which time` -f "Sort\t%U" ../YAILama Sort.bc
Sort    2.18
`which time` -f "Sort\t%U" lamac -i Sort.lama < /dev/null
Sort    4.10
```
