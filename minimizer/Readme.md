# Logic Minimizer for Microarchitectual Hash Functions
These are the python/sage scripts that allow to minimize microarchitectural hash functions.
In addition to these scripts, a running installation of the [espresso](https://github.com/classabbyamp/espresso-logic.git)[ logic minimizer](https://github.com/classabbyamp/espresso-logic.git)](https://github.com/classabbyamp/espresso-logic.git) is needed. 

# Dependencies
- [Espresso Logic Minimizer](https://github.com/classabbyamp/espresso-logic.git)
- [PuLP](https://pypi.org/project/PuLP/)
- [SageMath](https://doc.sagemath.org/html/en/installation/index.html)

# Running
After obtaining the `./measurements` folder using the tools in `../framework` the solver can be run using.
```
export ESPRESSO_EXECUTABLE=/path/to/espresso 
python3 read-and-minimize.py /path/to/measurements
```
In case the solver does not terminate it is also possible to reduce the number of bits and solve for a smaller function with the `limit-bits.py` script. 