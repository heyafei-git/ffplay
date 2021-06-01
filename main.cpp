//
// Created by dandy on 2021/6/1.
//

#include "filebroadcaster.h"

int main() {

    FileBroadCaster broadCaster("/Users/dandy/dandy/WebRTC/file01.mp4");

    broadCaster.start();

    return 0;
}