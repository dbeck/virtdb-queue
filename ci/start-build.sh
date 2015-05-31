#!/bin/sh

REPO="git@github.com:starschema/virtdb-queue.git"
BUILDNO=`git ls-remote --tags $REPO 2>&1 | ./new_build_tag.pl`
export BUILDNO
IMAGE_NAME="virtdb-build:centos6-queue-builder"
cat Dockerfile.in | sed s/"@BUILDNO@"/$BUILDNO/g > Dockerfile
docker build --force-rm=true -t "$IMAGE_NAME" .

if [ $? -ne 0 ]
then
  echo "ERROR during docker build " $IMAGE_NAME 
  exit 101
fi

echo "successfully built $IMAGE_NAME"

mkdir -p build-result
chmod a+rwxt build-result
docker run --rm=true -e "GITHUB_EMAIL=$GITHUB_EMAIL" \
                     -e "GITHUB_USER=$GITHUB_USER" \
                     -e "GITHUB_PASSWORD=$GITHUB_PASSWORD" \
                     -e "BUILDNO=$BUILDNO" \
                     -v $PWD/build-result:/home/virtdb-demo/build-result -t $IMAGE_NAME ./build-queue.sh $*
ls -ltr $PWD/build-result

