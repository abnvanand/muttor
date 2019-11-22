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
Filename
Filesize
Hash
tracker 1 Port
tracker 2 Port
...
tracker N Port
```

# TODO:
 - [x] Implement rarest first piece selection algorithm.
 - [x] Implement ack based piece transfer protocol.
 - [ ] Implement multi-tracker system
 - [ ] Switch to UDP for client-tracker and tracker-tracker communication.
 - [ ] Implement Thread Pool.
 - [ ] Implement DHT(Kademlia) ??