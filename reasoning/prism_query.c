#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <json-c/json.h>
    
#include "prism.h"

void seed_prism_vocabulary(void) {
    const char *vocab[] = {
        // 1. EVM Stack & Arithmetic
        "push", "pop", "dup", "swap", "add", "sub", "mul", "div", "sdiv", "mod", 
        "smod", "addmod", "mulmod", "exp", "signextend", "lt", "gt", "slt", "sgt", "eq",

        // 2. Logic & Bitwise
        "iszero", "and", "or", "xor", "not", "byte", "shl", "shr", "sar", "keccak256", 
        "sha3", "address", "balance", "origin", "caller", "callvalue", "calldataload", "calldatasize", "calldatacopy", "codesize",

        // 3. Memory & Storage Ops
        "codecopy", "gasprice", "extcodesize", "extcodecopy", "returndatasize", "returndatacopy", "extcodehash", "blockhash", "coinbase", "timestamp", 
        "number", "difficulty", "gaslimit", "chainid", "selfbalance", "basefee", "mload", "mstore", "mstore8", "sload",

        // 4. State & Execution Flow
        "sstore", "jump", "jumpi", "pc", "msize", "gas", "jumpdest", "tload", "tstore", "create", 
        "call", "callcode", "return", "delegatecall", "create2", "staticcall", "revert", "invalid", "selfdestruct", "stop",

        // 5. Higher-Level Syntax & Security
        "immutable", "constant", "virtual", "override", "indexed", "anonymous", "unchecked", "receive", "fallback", "constructor", 
        "salt", "offset", "length", "selector", "sig", "data", "stack", "heap", "pointer", "proxy",
    
        // 1. Logic & Conditionals
        "if", "then", "else", "when", "while", "for", "because", "not", "and", "or", 
        "true", "false", "equals", "matches", "exists", "becomes", "triggers", "causes", "prevents", "requires",
        
        // 2. Blockchain & Security
        "contract", "address", "balance", "sender", "owner", "caller", "value", "amount", "gas", "limit", 
        "transaction", "transfer", "mint", "burn", "deposit", "withdraw", "approve", "allowance", "mapping", "struct", 
        "array", "event", "emit", "revert", "require", "assert", "modifier", "internal", "public", "private", 
        "external", "view", "pure", "payable", "storage", "memory", "calldata", "library", "interface", "implementation",
        
        // 3. Vulnerability & Risk
        "reentrancy", "overflow", "underflow", "inflation", "rounding", "manipulation", "exploit", "attack", "vulnerability", "flaw", 
        "bug", "risk", "threat", "bypass", "unauthorized", "permanent", "failure", "collision", "delegatecall", "selfdestruct",
        
        // 4. Reasoning & Actions
        "calculate", "analyze", "verify", "check", "compare", "deduce", "simulate", "predict", "find", "scan", 
        "report", "audit", "explain", "summarize", "increase", "decrease", "update", "remove", "change", "result"
    };

    int total = sizeof(vocab) / sizeof(vocab[0]);
    printf("PRISM: Seeding %d core tokens...\n", total);

    for (int i = 0; i < total; i++) {
        insert_word(vocab[i]);
    }

    printf("Success: PRISM core vocabulary initialized.\n");
}

