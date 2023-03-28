# post-wod-wqe
Code example that shows how to use WOD WQE polling on DM

# Known issues
In CX-7 we have a HW BUG in which the HW upon execution of a WOD WQE polls the memory
and assumes it is written always in Big Endian.
Therefore, as a workaround we write the value in MEMIC in Big Endian in the first place