# mem_layout

Given a struct where its members have variable size such as this:

```c
typedef struct {
    int num_ints;
    int* ints;
    int num_floats;
    float* floats;
} var_struct;
```

`mem_layout` can be used to calculate and allocate a single buffer that fits the entire struct and its nested members.
See the example for more info.
