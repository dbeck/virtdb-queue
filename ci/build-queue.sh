#!/bin/bash

if [ "X" == "X$GITHUB_USER" ]; then echo "Need GITHUB_USER environment variable"; exit 10; fi
if [ "X" == "X$GITHUB_PASSWORD" ]; then echo "Need GITHUB_PASSWORD environment variable"; exit 10; fi
if [ "X" == "X$GITHUB_EMAIL" ]; then echo "Need GITHUB_EMAIL environment variable"; exit 10; fi
if [ "X" == "X$HOME" ]; then echo "Need HOME environment variable"; exit 10; fi
if [ "X" == "X$BUILDNO" ]; then echo "Need BUILDNO environment variable"; exit 10; fi

COMPONENT=virtdb-queue

cd $HOME

git clone --recursive https://$GITHUB_USER:$GITHUB_PASSWORD@github.com/starschema/$COMPONENT.git 
if [ $? -ne 0 ]; then echo "Failed to clone $COMPONENT repository"; exit 10; fi
echo Creating build

echo >>$HOME/.netrc
echo machine github.com >>$HOME/.netrc
echo login $GITHUB_USER >>$HOME/.netrc
echo password $GITHUB_PASSWORD >>$HOME/.netrc
echo >>$HOME/.netrc

cd $HOME/$COMPONENT

git --version
git config --global push.default simple
git config --global user.name $GITHUB_USER
git config --global user.email $GITHUB_EMAIL

gyp --depth=. -fmake 
make 
if [ $? -ne 0 ]; then echo "Failed to make $COMPONENT"; exit 10; fi

echo $BUILDNO
git tag -f $BUILDNO
if [ $? -ne 0 ]; then echo "Failed to tag repo"; exit 10; fi
git push origin $BUILDNO
if [ $? -ne 0 ]; then echo "Failed to push tag to repo."; exit 10; fi

out/Debug/queue_test
if [ $? -ne 0 ]; then echo "$COMPONENT tests failed"; exit 10; fi

