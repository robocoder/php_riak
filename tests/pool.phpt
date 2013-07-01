--TEST--
Test connection pool accepts limits
--INI--
riak.persistent.connections=20
riak.persistent.timeout=2000
--FILE--
<?php
include_once "connect.inc";

echo \Riak\PoolInfo::activeConnections().PHP_EOL;
$clients = array();
for ($i=0; $i<25; ++$i) {
	$clients[] = new \Riak\Connection($host, $port);
}
echo \Riak\PoolInfo::activeConnections().PHP_EOL;
echo \Riak\PoolInfo::activePersistentConnections().PHP_EOL;

$clients = NULL;

echo \Riak\PoolInfo::activeConnections().PHP_EOL;
echo \Riak\PoolInfo::activePersistentConnections().PHP_EOL;
echo \Riak\PoolInfo::getNumReconnects().PHP_EOL;
?>
--EXPECT--
0
25
20
0
0
0
