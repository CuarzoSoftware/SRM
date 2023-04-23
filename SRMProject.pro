TEMPLATE = subdirs

CONFIG -= qt
CONFIG -= app_bundle
CONFIG += ordered
CONFIG += c++11

SUBDIRS = src/lib/SRM.pro \
          src/examples/srm-all-connectors/srm-all-connectors.pro \
          src/examples/srm-display-info/srm-display-info.pro
