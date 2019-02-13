# rpc-broker

[![Build Status](https://travis-ci.org/tijko/xctools.svg?branch=master)](https://travis-ci.org/tijko/xctools)

## Usage
```
rpc-broker <flag> <argument>
        -b  [--bus-name=BUS]                    A dbus bus name to make the connection to.
        -h  [--help]                            Prints this usage description.
        -l  [--logging[=optional FILENAME]      Enables logging to a default path, optionally set.
        -p  [--policy-file=FILENAME]            Provide a policy file to run against.
        -r  [--raw-dbus=PORT]                   Sets rpc-broker to run on given port as raw DBus.
        -v  [--verbose]                         Adds extra information (run with logging).
        -w  [--websockets=PORT]                 Sets rpc-broker to run on given address/port as websockets.
```
## Description

rpc-broker is a DBus filtering system which allows or denys any messages that
are going to or coming from the bus. Based off a given policy, rpc-broker will
determine which messages are allowed to be communicated to DBus server.
rpc-broker also has the capability to filter messaging that is being sent in
response from the DBus.

## Policy

The policy file is what determines the filtering behavior of rpc-broker.  Given
a set of rules that either blacklist or whitelist certain messages.  The policy
can filter messages as fine grained as a specific method call or as broadly as
an entire destination. (The default behavior of rpc-broker is _deny all_).

#### policy syntax

Each rule in the policy **must** start with either `allow` or `deny`.  After
the filter policy of the rule there **must** be a `destination` specified
(as with `dbus-send` the destination is mandatory).  The filtering can be
determined just from these first two tokens, it does allow for more fine
grained control but isn't necessary.  You could allow/deny messages to all
communication coming to/from a given destination.  

## Examples

The following invocation will spawn an instance of rpc-broker that is listening
on port 5555 for raw dbus messaging, communicating any messaging to a dbus
server open on `/var/run/dbus/my-dbus-server`, and using the policy found under
path `/etc/my-policy`:

`rpc-broker -v -r 5555 -b /var/run/dbus/my-dbus-server \
                       -p /etc/my-policy`

A sample for what could be under `/etc/my-policy`:

    deny all
    allow destination org.freedesktop.DBus path /org/freedesktop/DBus interface org.freedesktop.DBus member Hello
    allow destination org.freedesktop.DBus path /org/freedesktop/DBus interface org.freedesktop.Introspectable member Introspect

With this simple policy, all messages will be denied, except for `Hello`
(needed to initiate communication with the DBus) and `Introspect`.  This
essentially means users are only allowed to introspect the DBus server.

## Testing
 
