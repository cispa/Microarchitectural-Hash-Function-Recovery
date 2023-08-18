# Artifacts for Efficient and Generic Microarchitectural Hash-Function Recovery
This repository contains the artifacts for the IEEE S&P 2024 paper "Efficient and Generic Microarchitectural Hash-Function Recovery".

# Structure of the Artifacts
The code is split across two folders both containing documentation on how to setup and run the code. 
- `./framework` contains all code that is necessary to measure microarchitectural hash functions reliably. If you want to measure a function on your computer you should head there to find out how to obtain the measurement data.
- `./minimizer` contains all necessary code to minimize a measured function. After obtaining the measurements using the framework they can be minimized here. 

## Citing Paper and Artifacts
If you use our results in your research, please cite our paper as:
```bibtex
@inproceedings{Gerlach2024Efficient,
 author={Gerlach, Lukas and Schwarz, Simon and Faro{\ss}, Nicolas and Schwarz, Michael},
 booktitle = {S\&P},
 title={Efficient and Generic Microarchitectural Hash-Function Recovery},
 year = {2024}
}

```
And our artifacts as:
```bibtex
@misc{Gerlach2024EfficientArtifacts,
 author={Gerlach, Lukas and Schwarz, Simon and Faro{\ss}, Nicolas and Schwarz, Michael},
 url = {https://github.com/cispa/Microarchitectural-Hash-Function-Recovery}
 title = {{Efficient and Generic Microarchitectural Hash-Function Recovery Artifact Repository}},
 year = {2024}
}
```
