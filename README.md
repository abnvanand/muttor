# How to run
## Build project
```shell script
make
```

## Run tracker
```shell script
./tracker myIp:myPort otherTrackerIp:otherTrackerPort
```
## Run clients
```shell script
./client myIp:myPort defaultTrackerIp:defaultTrackerPort
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
 - [x] Implement multi-tracker system
 - [x] Switch to UDP for tracker-tracker communication.
 - [ ] Implement Thread Pool.
 - [ ] Implement DHT(Kademlia) ??