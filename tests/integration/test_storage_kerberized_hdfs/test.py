import time
import pytest

import os

from helpers.cluster import ClickHouseCluster
import subprocess


cluster = ClickHouseCluster(__file__)
node1 = cluster.add_instance('node1', with_kerberized_hdfs=True, user_configs=[], main_configs=['configs/log_conf.xml', 'configs/hdfs.xml'])

@pytest.fixture(scope="module")
def started_cluster():
    try:
        cluster.start()

        yield cluster

    except Exception as ex:
        print(ex)
        raise ex
    finally:
        cluster.shutdown()

def test_read_table(started_cluster):
    # hdfs_api = HDFSApi("root")
    data = "1\tSerialize\t555.222\n2\tData\t777.333\n"
    started_cluster.hdfs_api.write_data("/simple_table_function", data)

    api_read = started_cluster.hdfs_api.read_data("/simple_table_function")
    print("api_read", api_read)

    assert api_read == data

    select_read = node1.query("select * from hdfs('hdfs://kerberizedhdfs1:9000/simple_table_function', 'TSV', 'id UInt64, text String, number Float64')")
    print("select_read", select_read)

    assert select_read == data


def test_read_write_storage(started_cluster):
    # node1.query("create table SimpleHDFSStorage (id UInt32, name String, weight Float64) ENGINE = HDFS('hdfs://kerberized_hdfs1.test.clickhouse.tech:9000/simple_storage', 'TSV')")
    node1.query("create table SimpleHDFSStorage2 (id UInt32, name String, weight Float64) ENGINE = HDFS('hdfs://kerberizedhdfs1:9000/simple_storage1', 'TSV')")
    node1.query("insert into SimpleHDFSStorage2 values (1, 'Mark', 72.53)")

    api_read = started_cluster.hdfs_api.read_data("/simple_storage1")
    print("api_read", api_read)
    assert api_read == "1\tMark\t72.53\n"

    select_read = node1.query("select * from SimpleHDFSStorage2")
    print("select_read", select_read)
    assert select_read == "1\tMark\t72.53\n"


def test_write_storage_expired(started_cluster):
    node1.query("create table SimpleHDFSStorageExpired (id UInt32, name String, weight Float64) ENGINE = HDFS('hdfs://kerberizedhdfs1:9000/simple_storage_expired', 'TSV')")

    time.sleep(45)   # wait for ticket expiration
    node1.query("insert into SimpleHDFSStorageExpired values (1, 'Mark', 72.53)")

    api_read = started_cluster.hdfs_api.read_data("/simple_storage_expired")
    print("api_read", api_read)
    assert api_read == "1\tMark\t72.53\n"

    select_read = node1.query("select * from SimpleHDFSStorageExpired")
    print("select_read", select_read)
    assert select_read == "1\tMark\t72.53\n"


def test_prohibited(started_cluster):
    node1.query("create table HDFSStorTwoProhibited (id UInt32, name String, weight Float64) ENGINE = HDFS('hdfs://suser@kerberizedhdfs1:9000/storage_user_two_prohibited', 'TSV')")
    try:
        node1.query("insert into HDFSStorTwoProhibited values (1, 'SomeOne', 74.00)")
        assert False, "Exception have to be thrown"
    except Exception as ex:
        assert "Unable to open HDFS file: /storage_user_two_prohibited error: Permission denied: user=specuser, access=WRITE" in str(ex)


def test_two_users(started_cluster):
    node1.query("create table HDFSStorOne (id UInt32, name String, weight Float64) ENGINE = HDFS('hdfs://kerberizedhdfs1:9000/storage_user_one', 'TSV')")
    node1.query("insert into HDFSStorOne values (1, 'IlyaReal', 86.00)")

    node1.query("create table HDFSStorTwo (id UInt32, name String, weight Float64) ENGINE = HDFS('hdfs://suser@kerberizedhdfs1:9000/user/specuser/storage_user_two', 'TSV')")
    node1.query("insert into HDFSStorTwo values (1, 'IlyaIdeal', 74.00)")

    select_read_1 = node1.query("select * from hdfs('hdfs://kerberizedhdfs1:9000/user/specuser/storage_user_two', 'TSV', 'id UInt64, text String, number Float64')")
    print("select_read_1", select_read_1)

    select_read_2 = node1.query("select * from hdfs('hdfs://suser@kerberizedhdfs1:9000/storage_user_one', 'TSV', 'id UInt64, text String, number Float64')")
    print("select_read_2", select_read_2)

    # node1.query("create table HDFSStorTwo_ (id UInt32, name String, weight Float64) ENGINE = HDFS('hdfs://kerberizedhdfs1:9000/user/specuser/storage_user_two', 'TSV')")
    # try:
    #     node1.query("insert into HDFSStorTwo_ values (1, 'AnotherPerspn', 88.54)")
    #     assert False, "Exception have to be thrown"
    # except Exception as ex:
    #     print ex
    #     assert "DB::Exception: Unable to open HDFS file: /user/specuser/storage_user_two error: Permission denied: user=root, access=WRITE, inode=\"/user/specuser/storage_user_two\":specuser:supergroup:drwxr-xr-x" in str(ex)


def test_cache_path(started_cluster):
    node1.query("create table HDFSStorCachePath (id UInt32, name String, weight Float64) ENGINE = HDFS('hdfs://dedicatedcachepath@kerberizedhdfs1:9000/storage_dedicated_cache_path', 'TSV')")
    try:
        node1.query("insert into HDFSStorCachePath values (1, 'FatMark', 92.53)")
        assert False, "Exception have to be thrown"
    except Exception as ex:
        assert "DB::Exception: hadoop.security.kerberos.ticket.cache.path cannot be set per user" in str(ex)



def test_read_table_not_expired(started_cluster):
    data = "1\tSerialize\t555.222\n2\tData\t777.333\n"
    started_cluster.hdfs_api.write_data("/simple_table_function_relogin", data)

    started_cluster.pause_container('hdfskerberos')
    time.sleep(45)

    try:
        select_read = node1.query("select * from hdfs('hdfs://reloginuser&kerberizedhdfs1:9000/simple_table_function', 'TSV', 'id UInt64, text String, number Float64')")
        assert False, "Exception have to be thrown"
    except Exception as ex:
        assert "DB::Exception: kinit failure:" in str(ex)

    started_cluster.unpause_container('hdfskerberos')



@pytest.mark.timeout(999999)
def _test_sleep_forever(started_cluster):
    time.sleep(999999)


if __name__ == '__main__':
    cluster.start()
    raw_input("Cluster created, press any key to destroy...")
    cluster.shutdown()
