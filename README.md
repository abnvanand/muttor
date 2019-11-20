# How to run
## Build project
```shell script
make
```

## Run tracker
```shell script
./tracker trackerPort
```
## Run clients
```shell script
./client clientPort tracker1Port tracker2Port
```

## Share file
```
share localFilePath mTorrentFilename 
```
## Get file
```
get mTorrentFilename fileNameToSave
```

# mtorrent file structure
```
tracker1Port
tracker2Port
Filename
Filesize
Hash
```

# TODO:
 - [x] Implement rarest first piece selection algorithm.
 - [x] Implement ack based piece transfer protocol.
 - [ ] Implement Thread Pool.
 - [ ] Switch to UDP for client-tracker and tracker-tracker communication.
 - [ ] Fault tolerant tracker.
