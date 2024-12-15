start_server {tags {"pkhash"}} {
    test {PKHSET/PKHLEN - Small pkhash creation} {
        array set smallhash {}
        for {set i 0} {$i < 8} {incr i} {
            set key [randstring 0 8 alpha]
            set val [randstring 0 8 alpha]
            if {[info exists smallhash($key)]} {
                incr i -1
                continue
            }
            r pkhset smallhash $key $val
            set smallhash($key) $val
        }
        list [r pkhlen smallhash]
    } {8}

# Pika does not support the debug command
   # test {Is the small pkhash encoded with a ziplist?} {
   #     assert_encoding ziplist smallhash
   # }
   #
    test {PKHSET/PKHLEN - Big pkhash creation} {
        array set bighash {}
        for {set i 0} {$i < 1024} {incr i} {
            set key [randstring 0 8 alpha]
            set val [randstring 0 8 alpha]
            if {[info exists bighash($key)]} {
                incr i -1
                continue
            }
            r pkhset bighash $key $val
            set bighash($key) $val
        }
        list [r pkhlen bighash]
    } {1024}

# Pika does not support the debug command
#    test {Is the big pkhash encoded with a ziplist?} {
#        assert_encoding hashtable bighash
#    }

    test {PKHGET against the small pkhash} {
        set err {}
        foreach k [array names smallhash *] {
            if {$smallhash($k) ne [r pkhget smallhash $k]} {
                set err "$smallhash($k) != [r pkhget smallhash $k]"
                break
            }
        }
        set _ $err
    } {}

    test {PKHGET against the big pkhash} {
        set err {}
        foreach k [array names bighash *] {
            if {$bighash($k) ne [r pkhget bighash $k]} {
                set err "$bighash($k) != [r pkhget bighash $k]"
                break
            }
        }
        set _ $err
    } {}

    test {PKHGET against non existing key} {
        set rv {}
        lappend rv [r pkhget smallhash __123123123__]
        lappend rv [r pkhget bighash __123123123__]
        set _ $rv
    } {{} {}}

    test {PKHSET in update and insert mode} {
        set rv {}
        set k [lindex [array names smallhash *] 0]
        lappend rv [r pkhset smallhash $k newval1]
        set smallhash($k) newval1
        lappend rv [r pkhget smallhash $k]
        lappend rv [r pkhset smallhash __foobar123__ newval]
        set k [lindex [array names bighash *] 0]
        lappend rv [r pkhset bighash $k newval2]
        set bighash($k) newval2
        lappend rv [r pkhget bighash $k]
        lappend rv [r pkhset bighash __foobar123__ newval]
        lappend rv [r pkhget smallhash __foobar123__]
        lappend rv [r pkhget bighash __foobar123__]
        set _ $rv
    } {0 newval1 1 0 newval2 1 1 1}
    # 没有的命令就删除掉，不支持。
    test {PKHSETNX target key missing - small pkhash} {
        r pkhsetnx smallhash __123123123__ foo
        r pkhget smallhash __123123123__
    } {foo}

    test {PKHSETNX target key exists - small pkhash} {
        r pkhsetnx smallhash __123123123__ bar
        set result [r pkhget smallhash __123123123__]
        r pkhget smallhash __123123123__
        set _ $result
    } {foo}

    test {PKHSETNX target key missing - big pkhash} {
        r pkhsetnx bighash __123123123__ foo
        r pkhget bighash __123123123__
    } {foo}

    test {PKHSETNX target key exists - big pkhash} {
        r pkhsetnx bighash __123123123__ bar
        set result [r pkhget bighash __123123123__]
        r pkhget bighash __123123123__
        set _ $result
    } {foo}

    test {PKHMSET wrong number of args} {
        catch {r pkhmset smallhash key1 val1 key2} err
        format $err
    } {*wrong number*}

    test {PKHMSET - small pkhash} {
        set args {}
        foreach {k v} [array get smallhash] {
            set newval [randstring 0 8 alpha]
            set smallhash($k) $newval
            lappend args $k $newval
        }
        r pkhmset smallhash {*}$args
    } {OK}

    test {PKHMSET - big pkhash} {
        set args {}
        foreach {k v} [array get bighash] {
            set newval [randstring 0 8 alpha]
            set bighash($k) $newval
            lappend args $k $newval
        }
        r pkhmset bighash {*}$args
    } {OK}

    test {PKHMGET against non existing key and fields} {
        set rv {}
        lappend rv [r pkhmget doesntexist __123123123__ __456456456__]
        lappend rv [r pkhmget smallhash __123123123__ __456456456__]
        lappend rv [r pkhmget bighash __123123123__ __456456456__]
        set _ $rv
    } {{{} {}} {{} {}} {{} {}}}

# Keys for multiple data types of Pika can be duplicate
    test {PKHMGET against wrong type} {
        r set wrongtype somevalue
        assert_error "*wrong*" {r pkhmget wrongtype field1 field2}
    }

    test {PKHMGET - small pkhash} {
        set keys {}
        set vals {}
        foreach {k v} [array get smallhash] {
            lappend keys $k
            lappend vals $v
        }
        set err {}
        set result [r pkhmget smallhash {*}$keys]
        if {$vals ne $result} {
            set err "$vals != $result"
            break
        }
        set _ $err
    } {}

    test {PKHMGET - big pkhash} {
        set keys {}
        set vals {}
        foreach {k v} [array get bighash] {
            lappend keys $k
            lappend vals $v
        }
        set err {}
        set result [r pkhmget bighash {*}$keys]
        if {$vals ne $result} {
            set err "$vals != $result"
            break
        }
        set _ $err
    } {}

    test {PKHKEYS - small pkhash} {
        lsort [r pkhkeys smallhash]
    } [lsort [array names smallhash *]]

    test {PKHKEYS - big pkhash} {
        lsort [r pkhkeys bighash]
    } [lsort [array names bighash *]]

    test {PKHVALS - small pkhash} {
        set vals {}
        foreach {k v} [array get smallhash] {
            lappend vals $v
        }
        set _ [lsort $vals]
    } [lsort [r pkhvals smallhash]]

    test {PKHVALS - big pkhash} {
        set vals {}
        foreach {k v} [array get bighash] {
            lappend vals $v
        }
        set _ [lsort $vals]
    } [lsort [r pkhvals bighash]]

    test {PKHGETALL - small pkhash} {
        lsort [r pkhgetall smallhash]
    } [lsort [array get smallhash]]

    test {PKHGETALL - big pkhash} {
        lsort [r pkhgetall bighash]
    } [lsort [array get bighash]]

    test {PKHDEL and return value} {
        set rv {}
        lappend rv [r pkhget smallhash nokey]
        lappend rv [r pkhget bighash nokey]
        set k [lindex [array names smallhash *] 0]
        lappend rv [r pkhget smallhash $k]
        lappend rv [r pkhget smallhash $k]
        lappend rv [r pkhget smallhash $k]
        unset smallhash($k)
        set k [lindex [array names bighash *] 0]
        lappend rv [r pkhget bighash $k]
        lappend rv [r pkhget bighash $k]
        lappend rv [r pkhget bighash $k]
        unset bighash($k)
        set _ $rv
    } {0 0 1 0 {} 1 0 {}}

    test {PKHDEL - more than a single value} {
        set rv {}
        r del myhash
        r pkhmset myhash a 1 b 2 c 3
        assert_equal 0 [r pkhget myhash x y]
        assert_equal 2 [r pkhget myhash a c f]
        r pkhgetall myhash
    } {b 2}

    test {PKHDEL - pkhash becomes empty before deleting all specified fields} {
        r del myhash
        r pkhmset myhash a 1 b 2 c 3
        assert_equal 3 [r pkhget myhash a b c d e]
        assert_equal 0 [r exists myhash]
    }

    test {PKHEXISTS} {
        set rv {}
        set k [lindex [array names smallhash *] 0]
        lappend rv [r pkhexists smallhash $k]
        lappend rv [r pkhexists smallhash nokey]
        set k [lindex [array names bighash *] 0]
        lappend rv [r pkhexists bighash $k]
        lappend rv [r pkhexists bighash nokey]
    } {1 0 1 0}

# Pika does not support the debug command
#    test {Is a ziplist encoded Hash promoted on big payload?} {
#        r pkhset smallhash foo [string repeat a 1024]
#        r debug object smallhash
#    } {*hashtable*}

    test {PKHINCRBY against non existing database key} {
        r del htest
        list [r pkhincrby htest foo 2]
    } {2}

    test {PKHINCRBY against non existing pkhash key} {
        set rv {}
        r pkhget smallhash tmp
        r pkhget bighash tmp
        lappend rv [r pkhincrby smallhash tmp 2]
        lappend rv [r pkhget smallhash tmp]
        lappend rv [r pkhincrby bighash tmp 2]
        lappend rv [r pkhget bighash tmp]
    } {2 2 2 2}

    test {PKHINCRBY against pkhash key created by pkhincrby itself} {
        set rv {}
        lappend rv [r pkhincrby smallhash tmp 3]
        lappend rv [r pkhget smallhash tmp]
        lappend rv [r pkhincrby bighash tmp 3]
        lappend rv [r pkhget bighash tmp]
    } {5 5 5 5}

    test {PKHINCRBY against pkhash key originally set with PKHSET} {
        r pkhset smallhash tmp 100
        r pkhset bighash tmp 100
        list [r pkhincrby smallhash tmp 2] [r pkhincrby bighash tmp 2]
    } {102 102}

    test {PKHINCRBY over 32bit value} {
        r pkhset smallhash tmp 17179869184
        r pkhset bighash tmp 17179869184
        list [r pkhincrby smallhash tmp 1] [r pkhincrby bighash tmp 1]
    } {17179869185 17179869185}

    test {PKHINCRBY over 32bit value with over 32bit increment} {
        r pkhset smallhash tmp 17179869184
        r pkhset bighash tmp 17179869184
        list [r pkhincrby smallhash tmp 17179869184] [r pkhincrby bighash tmp 17179869184]
    } {34359738368 34359738368}

    test {PKHINCRBY fails against pkhash value with spaces (left)} {
        r pkhset smallhash str " 11"
        r pkhset bighash str " 11"
        catch {r pkhincrby smallhash str 1} smallerr
        catch {r pkhincrby smallhash str 1} bigerr
        set rv {}
        lappend rv [string match "ERR*not an integer*" $smallerr]
        lappend rv [string match "ERR*not an integer*" $bigerr]
    } {1 1}

    test {PKHINCRBY fails against pkhash value with spaces (right)} {
        r pkhset smallhash str "11 "
        r pkhset bighash str "11 "
        catch {r pkhincrby smallhash str 1} smallerr
        catch {r pkhincrby smallhash str 1} bigerr
        set rv {}
        lappend rv [string match "ERR*not an integer*" $smallerr]
        lappend rv [string match "ERR*not an integer*" $bigerr]
    } {1 1}

    test {PKHINCRBY can detect overflows} {
        set e {}
        r pkhset pkhash n -9223372036854775484
        assert {[r pkhincrby pkhash n -1] == -9223372036854775485}
        catch {r pkhincrby pkhash n -10000} e
        set e
    } {*overflow*}
    # DDD 确定一下是否有实现？ 
    # test {PKHINCRBYFLOAT against non existing database key} {
    #     r del htest
    #     list [r hincrbyfloat htest foo 2.5]
    # } {2.5}

    # test {PKHINCRBYFLOAT against non existing pkhash key} {
    #     set rv {}
    #     r pkhget smallhash tmp
    #     r pkhget bighash tmp
    #     lappend rv [roundFloat [r hincrbyfloat smallhash tmp 2.5]]
    #     lappend rv [roundFloat [r pkhget smallhash tmp]]
    #     lappend rv [roundFloat [r hincrbyfloat bighash tmp 2.5]]
    #     lappend rv [roundFloat [r pkhget bighash tmp]]
    # } {2.5 2.5 2.5 2.5}

    # test {PKHINCRBYFLOAT against pkhash key created by pkhincrby itself} {
    #     set rv {}
    #     lappend rv [roundFloat [r hincrbyfloat smallhash tmp 3.5]]
    #     lappend rv [roundFloat [r pkhget smallhash tmp]]
    #     lappend rv [roundFloat [r hincrbyfloat bighash tmp 3.5]]
    #     lappend rv [roundFloat [r pkhget bighash tmp]]
    # } {6 6 6 6}

    # test {PKHINCRBYFLOAT against pkhash key originally set with PKHSET} {
    #     r pkhset smallhash tmp 100
    #     r pkhset bighash tmp 100
    #     list [roundFloat [r hincrbyfloat smallhash tmp 2.5]] \
    #          [roundFloat [r hincrbyfloat bighash tmp 2.5]]
    # } {102.5 102.5}

    # test {PKHINCRBYFLOAT over 32bit value} {
    #     r pkhset smallhash tmp 17179869184
    #     r pkhset bighash tmp 17179869184
    #     list [r hincrbyfloat smallhash tmp 1] \
    #          [r hincrbyfloat bighash tmp 1]
    # } {17179869185 17179869185}

    # test {PKHINCRBYFLOAT over 32bit value with over 32bit increment} {
    #     r pkhset smallhash tmp 17179869184
    #     r pkhset bighash tmp 17179869184
    #     list [r hincrbyfloat smallhash tmp 17179869184] \
    #          [r hincrbyfloat bighash tmp 17179869184]
    # } {34359738368 34359738368}

    # test {PKHINCRBYFLOAT fails against pkhash value with spaces (left)} {
    #     r pkhset smallhash str " 11"
    #     r pkhset bighash str " 11"
    #     catch {r hincrbyfloat smallhash str 1} smallerr
    #     catch {r hincrbyfloat smallhash str 1} bigerr
    #     set rv {}
    #     lappend rv [string match "ERR*not*float*" $smallerr]
    #     lappend rv [string match "ERR*not*float*" $bigerr]
    # } {1 1}

    # test {PKHINCRBYFLOAT fails against pkhash value with spaces (right)} {
    #     r pkhset smallhash str "11 "
    #     r pkhset bighash str "11 "
    #     catch {r hincrbyfloat smallhash str 1} smallerr
    #     catch {r hincrbyfloat smallhash str 1} bigerr
    #     set rv {}
    #     lappend rv [string match "ERR*not*float*" $smallerr]
    #     lappend rv [string match "ERR*not*float*" $bigerr]
    # } {1 1}

    test {Hash ziplist regression test for large keys} {
        r pkhset pkhash kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk a
        r pkhset pkhash kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk b
        r pkhget pkhash kkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkkk
    } {b}

    foreach size {10 512} {
        test "Hash fuzzing #1 - $size fields" {
            for {set times 0} {$times < 10} {incr times} {
                catch {unset pkhash}
                array set pkhash {}
                r del pkhash

                # Create
                for {set j 0} {$j < $size} {incr j} {
                    set field [randomValue]
                    set value [randomValue]
                    r pkhset pkhash $field $value
                    set pkhash($field) $value
                }

                # Verify
                foreach {k v} [array get pkhash] {
                    assert_equal $v [r pkhget pkhash $k]
                }
                assert_equal [array size pkhash] [r pkhlen pkhash]
            }
        }

        test "Hash fuzzing #2 - $size fields" {
            for {set times 0} {$times < 10} {incr times} {
                catch {unset pkhash}
                array set pkhash {}
                r del pkhash

                # Create
                for {set j 0} {$j < $size} {incr j} {
                    randpath {
                        set field [randomValue]
                        set value [randomValue]
                        r pkhset pkhash $field $value
                        set pkhash($field) $value
                    } {
                        set field [randomSignedInt 512]
                        set value [randomSignedInt 512]
                        r pkhset pkhash $field $value
                        set pkhash($field) $value
                    } {
                        randpath {
                            set field [randomValue]
                        } {
                            set field [randomSignedInt 512]
                        }
                        r pkhget pkhash $field
                        unset -nocomplain pkhash($field)
                    }
                }

                # Verify
                foreach {k v} [array get pkhash] {
                    assert_equal $v [r pkhget pkhash $k]
                }
                assert_equal [array size pkhash] [r pkhlen pkhash]
            }
        }
    }

# This parameter is not available in Pika
#    The pkhash-max-ziplist-entries parameter is not available in Pika
#    test {Stress test the pkhash ziplist -> hashtable encoding conversion} {
#        r config set pkhash-max-ziplist-entries 32
#        for {set j 0} {$j < 100} {incr j} {
#            r del myhash
#            for {set i 0} {$i < 64} {incr i} {
#                r pkhset myhash [randomValue] [randomValue]
#            }
#            assert {[r object encoding myhash] eq {hashtable}}
#        }
#    }
}
