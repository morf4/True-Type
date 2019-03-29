//+------------------------------------------------------------------------------ 
//
//  Copyright (C) Microsoft Corporation
//
//  File: ttmem.cpp 
//
//  Description: 
//      Routines to allocate and free memory. 
//
//  History: 
//      02/12/2004 [....]
//          Created it.
//
//----------------------------------------------------------------------------- 

#include "typedefs.h" 
 
#include "ttmem.h"
 
#include "fsassert.h"

using namespace System::Security;
using namespace System::Security::Permissions; 
using namespace System::Runtime::InteropServices;
 
// <SecurityNote> 
//  Critical - because there is an elevation on the function.
//  TreatAsSafe - because memory allocations can at best cause an app to run out memory 
//                which can be done anyway by just allocating tons of objects.
// </SecurityNote>
[SecurityCritical, SecurityTreatAsSafe]
[SecurityPermission(SecurityAction::Assert, UnmanagedCode = true)] 
void * Mem_Alloc(size_t size)
{ 
    return calloc(1, size); 
}
 

// <SecurityNote>
//  Critical - because there is an elevation on the function.
//  TreatAsSafe - safe because you need to be in an unsafe block to play with pointers 
//                which you can't in a partial trust app.  An extremely far fetched exploit
//                would be for someone to twist our code to free a managed object that has 
//                security sensitive information using free and hope that something weird 
//                happens.  It really shouldn't happen unless someone puts malicious code in
//                our code base which is not really a protectable scenario. 
// </SecurityNote>
[SecurityCritical, SecurityTreatAsSafe]
[SecurityPermission(SecurityAction::Assert, UnmanagedCode = true)]
void Real_Mem_Free(void * pv) 
{
    free (pv); 
} 

 
// Mem_Free/Mem_Alloc are expensive in partial trust. More than half of the calls to Mem_Free are
// with NULL pointers. So we check for NULL pointer before going into expensive assert and interop.
// There are more optimizations possible (for example grouping Mem_Alloc calls). But this is safe.
void Mem_Free(void * pv) 
{
    if (pv != NULL) 
    { 
        Real_Mem_Free(pv);
    } 
}


// <SecurityNote> 
//  Critical - because there is an elevation on the function.
//  TreatAsSafe - because memory allocations can at best cause an app to run out memory 
//                which can be done anyway by just allocating tons of objects.  Also 
//                safe because you need to be in an unsafe block to play with pointers
//                which you can't in a partial trust app. 
// </SecurityNote>
[SecurityCritical, SecurityTreatAsSafe]
[SecurityPermission(SecurityAction::Assert, UnmanagedCode = true)]
void * Mem_ReAlloc(void * base, size_t newSize) 
{
    return realloc(base, newSize); 
} 

int16 Mem_Init(void) 
{
    return MemNoErr;
}
 
void Mem_End(void)
{ 
} 


// File provided for Reference Use Only by Microsoft Corporation (c) 2007.
// Copyright (c) Microsoft Corporation. All rights reserved.
//+------------------------------------------------------------------------------ 
//
//  Copyright (C) Microsoft Corporation
//
//  File: ttmem.cpp 
//
//  Description: 
//      Routines to allocate and free memory. 
//
//  History: 
//      02/12/2004 [....]
//          Created it.
//
//----------------------------------------------------------------------------- 

#include "typedefs.h" 
 
#include "ttmem.h"
 
#include "fsassert.h"

using namespace System::Security;
using namespace System::Security::Permissions; 
using namespace System::Runtime::InteropServices;
 
// <SecurityNote> 
//  Critical - because there is an elevation on the function.
//  TreatAsSafe - because memory allocations can at best cause an app to run out memory 
//                which can be done anyway by just allocating tons of objects.
// </SecurityNote>
[SecurityCritical, SecurityTreatAsSafe]
[SecurityPermission(SecurityAction::Assert, UnmanagedCode = true)] 
void * Mem_Alloc(size_t size)
{ 
    return calloc(1, size); 
}
 

// <SecurityNote>
//  Critical - because there is an elevation on the function.
//  TreatAsSafe - safe because you need to be in an unsafe block to play with pointers 
//                which you can't in a partial trust app.  An extremely far fetched exploit
//                would be for someone to twist our code to free a managed object that has 
//                security sensitive information using free and hope that something weird 
//                happens.  It really shouldn't happen unless someone puts malicious code in
//                our code base which is not really a protectable scenario. 
// </SecurityNote>
[SecurityCritical, SecurityTreatAsSafe]
[SecurityPermission(SecurityAction::Assert, UnmanagedCode = true)]
void Real_Mem_Free(void * pv) 
{
    free (pv); 
} 

 
// Mem_Free/Mem_Alloc are expensive in partial trust. More than half of the calls to Mem_Free are
// with NULL pointers. So we check for NULL pointer before going into expensive assert and interop.
// There are more optimizations possible (for example grouping Mem_Alloc calls). But this is safe.
void Mem_Free(void * pv) 
{
    if (pv != NULL) 
    { 
        Real_Mem_Free(pv);
    } 
}


// <SecurityNote> 
//  Critical - because there is an elevation on the function.
//  TreatAsSafe - because memory allocations can at best cause an app to run out memory 
//                which can be done anyway by just allocating tons of objects.  Also 
//                safe because you need to be in an unsafe block to play with pointers
//                which you can't in a partial trust app. 
// </SecurityNote>
[SecurityCritical, SecurityTreatAsSafe]
[SecurityPermission(SecurityAction::Assert, UnmanagedCode = true)]
void * Mem_ReAlloc(void * base, size_t newSize) 
{
    return realloc(base, newSize); 
} 

int16 Mem_Init(void) 
{
    return MemNoErr;
}
 
void Mem_End(void)
{ 
} 


// File provided for Reference Use Only by Microsoft Corporation (c) 2007.
// Copyright (c) Microsoft Corporation. All rights reserved.
