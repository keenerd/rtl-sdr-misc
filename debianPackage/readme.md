

Step 1, make sure you have the required dependencys : 
```sudo apt-get install build-essential```

Step 2, copy the compiled binary into the package location (implying rtl-ais have been compiled already): 
```mkdir -p ./usr/bin && cp ../rtl_ais ./usr/bin/rtl_ais```

Step 3, build the package : 
```dpkg-deb --build ./ rtl_ais_0.3-1_$(arch).deb```
