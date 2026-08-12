cmake_minimum_required(VERSION 3.16)

project(dde_shell_dock_left_demo VERSION 1.0 LANGUAGES CXX)

find_package(DDEShell REQUIRED)

ds_install_package(PACKAGE org.deepin.ds.dock-left-demo)
