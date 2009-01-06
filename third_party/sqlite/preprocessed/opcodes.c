/* Automatically generated.  Do not edit */
/* See the mkopcodec.awk script for details. */
#if !defined(SQLITE_OMIT_EXPLAIN) || !defined(NDEBUG) || defined(VDBE_PROFILE) || defined(SQLITE_DEBUG)
const char *sqlite3OpcodeName(int i){
 static const char *const azName[] = { "?",
     /*   1 */ "VNext",
     /*   2 */ "Affinity",
     /*   3 */ "Column",
     /*   4 */ "SetCookie",
     /*   5 */ "Sequence",
     /*   6 */ "MoveGt",
     /*   7 */ "RowKey",
     /*   8 */ "SCopy",
     /*   9 */ "OpenWrite",
     /*  10 */ "If",
     /*  11 */ "VRowid",
     /*  12 */ "CollSeq",
     /*  13 */ "OpenRead",
     /*  14 */ "Expire",
     /*  15 */ "AutoCommit",
     /*  16 */ "Not",
     /*  17 */ "Pagecount",
     /*  18 */ "IntegrityCk",
     /*  19 */ "Sort",
     /*  20 */ "Copy",
     /*  21 */ "Trace",
     /*  22 */ "Function",
     /*  23 */ "IfNeg",
     /*  24 */ "Noop",
     /*  25 */ "Return",
     /*  26 */ "NewRowid",
     /*  27 */ "Variable",
     /*  28 */ "String",
     /*  29 */ "RealAffinity",
     /*  30 */ "VRename",
     /*  31 */ "ParseSchema",
     /*  32 */ "VOpen",
     /*  33 */ "Close",
     /*  34 */ "CreateIndex",
     /*  35 */ "IsUnique",
     /*  36 */ "NotFound",
     /*  37 */ "Int64",
     /*  38 */ "MustBeInt",
     /*  39 */ "Halt",
     /*  40 */ "Rowid",
     /*  41 */ "IdxLT",
     /*  42 */ "AddImm",
     /*  43 */ "Statement",
     /*  44 */ "RowData",
     /*  45 */ "MemMax",
     /*  46 */ "NotExists",
     /*  47 */ "Gosub",
     /*  48 */ "Integer",
     /*  49 */ "Prev",
     /*  50 */ "VColumn",
     /*  51 */ "CreateTable",
     /*  52 */ "Last",
     /*  53 */ "IncrVacuum",
     /*  54 */ "IdxRowid",
     /*  55 */ "ResetCount",
     /*  56 */ "FifoWrite",
     /*  57 */ "ContextPush",
     /*  58 */ "Yield",
     /*  59 */ "DropTrigger",
     /*  60 */ "Or",
     /*  61 */ "And",
     /*  62 */ "DropIndex",
     /*  63 */ "IdxGE",
     /*  64 */ "IdxDelete",
     /*  65 */ "IsNull",
     /*  66 */ "NotNull",
     /*  67 */ "Ne",
     /*  68 */ "Eq",
     /*  69 */ "Gt",
     /*  70 */ "Le",
     /*  71 */ "Lt",
     /*  72 */ "Ge",
     /*  73 */ "Vacuum",
     /*  74 */ "BitAnd",
     /*  75 */ "BitOr",
     /*  76 */ "ShiftLeft",
     /*  77 */ "ShiftRight",
     /*  78 */ "Add",
     /*  79 */ "Subtract",
     /*  80 */ "Multiply",
     /*  81 */ "Divide",
     /*  82 */ "Remainder",
     /*  83 */ "Concat",
     /*  84 */ "MoveLe",
     /*  85 */ "IfNot",
     /*  86 */ "DropTable",
     /*  87 */ "BitNot",
     /*  88 */ "String8",
     /*  89 */ "MakeRecord",
     /*  90 */ "ResultRow",
     /*  91 */ "Delete",
     /*  92 */ "AggFinal",
     /*  93 */ "Compare",
     /*  94 */ "Goto",
     /*  95 */ "TableLock",
     /*  96 */ "FifoRead",
     /*  97 */ "Clear",
     /*  98 */ "MoveLt",
     /*  99 */ "VerifyCookie",
     /* 100 */ "AggStep",
     /* 101 */ "SetNumColumns",
     /* 102 */ "Transaction",
     /* 103 */ "VFilter",
     /* 104 */ "VDestroy",
     /* 105 */ "ContextPop",
     /* 106 */ "Next",
     /* 107 */ "IdxInsert",
     /* 108 */ "Insert",
     /* 109 */ "Destroy",
     /* 110 */ "ReadCookie",
     /* 111 */ "ForceInt",
     /* 112 */ "LoadAnalysis",
     /* 113 */ "Explain",
     /* 114 */ "OpenPseudo",
     /* 115 */ "OpenEphemeral",
     /* 116 */ "Null",
     /* 117 */ "Move",
     /* 118 */ "Blob",
     /* 119 */ "Rewind",
     /* 120 */ "MoveGe",
     /* 121 */ "VBegin",
     /* 122 */ "VUpdate",
     /* 123 */ "IfZero",
     /* 124 */ "VCreate",
     /* 125 */ "Real",
     /* 126 */ "Found",
     /* 127 */ "IfPos",
     /* 128 */ "NullRow",
     /* 129 */ "Jump",
     /* 130 */ "Permutation",
     /* 131 */ "NotUsed_131",
     /* 132 */ "NotUsed_132",
     /* 133 */ "NotUsed_133",
     /* 134 */ "NotUsed_134",
     /* 135 */ "NotUsed_135",
     /* 136 */ "NotUsed_136",
     /* 137 */ "NotUsed_137",
     /* 138 */ "ToText",
     /* 139 */ "ToBlob",
     /* 140 */ "ToNumeric",
     /* 141 */ "ToInt",
     /* 142 */ "ToReal",
  };
  return azName[i];
}
#endif
