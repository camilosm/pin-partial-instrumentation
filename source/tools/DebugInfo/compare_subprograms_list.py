#!/usr/intel/bin/python

#
# Copyright (C) 2022-2022 Intel Corporation.
# SPDX-License-Identifier: MIT
#


import argparse
import os
import sys
import subprocess

def compare_files(reference_file, pindwarf_file):
    """
    This function gets two files in the same format.
    The reference file was generated by the parse_dwarfdump_output.py script.
    The pindwarf file was generated by the pindwarf.cpp pintool.
    The diff command will generate output in diff format. The diff output is parsed to extract
    all the addresses for which there is a diff.
    For each address, we grep the address in the reference file and in the pindwarf file.
    If there is only one line and it is in the diff then this line is different and the comparison will return with false.
    It is possible that the same address will have more than one unmangled name in the DWARf, for example lambda functions.
    However each address will have just one mangled name.
    The LLVM parser returns only one such function per a pair of address and mangled.
    The success criteria is that the function returned by the LLVM parser is contained in the list of functions in the reference file,
    so that every address is covered in the list provided by the LLVM parser.
    """
    print("comparing [ %s ] with [ %s ]" % (reference_file, pindwarf_file))
    compare_ok = True
    addresses = []
    cmd = 'diff {} {}'.format(reference_file, pindwarf_file)
    p = subprocess.Popen(cmd, shell=True, universal_newlines=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    while True:
        line = p.stdout.readline()
        if line.startswith(('<' , '>')):
            address = line.split()[1].lstrip('<').lstrip('>').lstrip()
            addresses.append(address)
        if not line and p.poll() is not None:
            break
    addresses = list(set(addresses)) # Remove duplicates
    addresses.sort()
    for address in addresses:
        cmd = 'grep {} {}'.format(address, reference_file)
        p = subprocess.run(cmd, shell=True, capture_output=True)
        assert p.returncode == 0, "\"{}\" failed".format(cmd)
        lines_ref = p.stdout.decode("utf-8").strip().split('\n')

        cmd = 'grep {} {}'.format(address, pindwarf_file)
        p = subprocess.run(cmd, shell=True, capture_output=True)
        assert p.returncode == 0, "\"{}\" failed".format(cmd)
        lines_pindwarf = p.stdout.decode("utf-8").strip().split('\n')

        if len(lines_ref) == 1 and len(lines_pindwarf) == 1:
            print(" => FAIL : 0x{} : some attributes for the function in this address are different ".format(address))
            print("\t\t%-30s: %s" % (reference_file, lines_ref[0]))
            print("\t\t%-30s: %s" % (pindwarf_file, lines_pindwarf[0]))
            compare_ok = False
        else:
            if set(lines_pindwarf).issubset(set(lines_ref)):
                print(" => PASS : 0x{} : pindwarf functions are a subset of the reference functions for this address".format(address))
            else:
                print(" => FAIL : 0x{} : pindwarf functions have an item that is missing in the reference functions for this address".format(address))
                compare_ok = False
    return compare_ok
    
def parse_args():
    parser = argparse.ArgumentParser(formatter_class=argparse.RawDescriptionHelpFormatter,
                                     description="Executable tool for comparing two files containing list of subprograms\n"
                                     )
    parser.add_argument('--reference', help='Reference file containing a list of subprograms')
    parser.add_argument('--pindwarf', help='File generated by using pindwarf library containing a list of subprograms')
    args = parser.parse_args()

    if not args.reference:
        print('Missing --reference')
        return None
    if not args.pindwarf:
        print('Missing --pindwarf')
        return None
    if not os.path.exists(args.reference):
        print("File does not exist ({})".format(args.reference))
        return None
    if not os.path.exists(args.pindwarf):
        print("File does not exist ({})".format(args.pindwarf))
        return None
    return args

def main():
    args = parse_args()
    if not args:
        return False
    return compare_files(args.reference, args.pindwarf)

# main function
if __name__ == "__main__":
    sys.exit( 0 if main() else 1)
