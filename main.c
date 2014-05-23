#include <stdio.h>
#include <assert.h>
#include <string.h>

#include <pow.h> 

void  printObjInfo( PWR_Obj );
void  traverseDepth( PWR_Obj );
const char * attrUnit( PWR_AttrUnits );
const char * attrValueType( PWR_AttrDataType type );

void  printAllAttr( PWR_Obj );

const char* getObjName( PWR_Obj obj );

int main( int argc, char* argv[] )
{
    int num;
    int ret;
    PWR_Grp group;
    PWR_Obj object;

    // Get a context
    PWR_Cntxt context = PWR_CntxtInit( PWR_CNTXT_DEFAULT, PWR_ROLE_APP, "App" );; 

    assert( context );
    object = PWR_CntxtGetSelf( context );
    assert( object );

    // Get a group that we can add stuff to
    PWR_Grp userGrp = PWR_CntxtCreateGrp( context, "userGrp" );


#if 0 
    traverseDepth( PWR_CntxtGetSelf( context ) );
#endif
    
    ret = PWR_AppHint( object, PWR_REGION_SERIAL ); 
    assert( ret == PWR_ERR_SUCCESS );
    /* would normally check return */

    // Get all of the CORE objects
    group = PWR_CntxtGetGrpByType( context, PWR_OBJ_CORE );
    assert( PWR_NULL != group );

    // Iterate over all objects in this group
    num = PWR_GrpGetNumObjs( group ); 
    int i;
    for ( i = 0; i < num; i++ ) {
        
        PWR_Obj obj = PWR_GrpGetObjByIndx( group, i );
        printf("Obj `%s` type=`%s`\n", getObjName(obj), 
		        PWR_ObjGetTypeString(PWR_ObjGetType( obj ) ) );

        printAllAttr( obj );
        printf("\n");

        // Add the object to another group
        PWR_GrpAddObj( userGrp, obj ); 
    }

    // Get all of the NODE objects
    group = PWR_CntxtGetGrpByType( context, PWR_OBJ_NODE );
    assert( PWR_NULL != group );

    // use index based interation to loop through all objects in the group
    num = PWR_GrpGetNumObjs( group );
    for ( i = 0; i < num; i++ ) {

        // get the object at index N
        PWR_Obj tmp = PWR_GrpGetObjByIndx(group, i );

        // Add the object to another group
        PWR_GrpAddObj( userGrp, tmp ); 

        // Get the object parent(s)
        PWR_Obj parent = PWR_ObjGetParent( tmp );
        if ( PWR_NULL != parent ) {
            char pstr[100]; 
            char cstr[100];
            strcpy( pstr, getObjName( parent ) );
            strcpy( cstr, getObjName( tmp ) );
            printf("parent %s, child %s\n", pstr, cstr ); 
                
        } else {
            printf("%s\n", getObjName( tmp ) ); 
        }
    } 

    PWR_Grp tmpGrp = PWR_CntxtGetGrpByName( context, "userGrp" );
    // print all of the objects in the user created group

    num = PWR_GrpGetNumObjs( tmpGrp );
    for ( i = 0; i < num; i++ ) {  
        printf("userGrp %s\n", getObjName(
		 PWR_GrpGetObjByIndx( tmpGrp, i ) ) );
    } 

    // cleanup
    assert( PWR_ERR_SUCCESS == PWR_GrpDestroy( userGrp ) );
    assert( PWR_ERR_SUCCESS == PWR_CntxtDestroy( context ) );
    return 0;
}

void  traverseDepth( PWR_Obj obj )
{
    printObjInfo( obj );

    PWR_Grp children = PWR_ObjGetChildren( obj );

    if ( children ) {
        int num = PWR_GrpGetNumObjs( children );
        int i;
        for ( i = 0; i < num; i++ ) {
            traverseDepth( PWR_GrpGetObjByIndx( children, i ) );
        }
    } 
}

void  printObjInfo( PWR_Obj obj ) {
    const char* name = getObjName( obj );	
    const char* type = PWR_ObjGetTypeString( PWR_ObjGetType( obj ) );
    printf( "object %s is of type %s\n", name, type );
}


static void  printObjAttr( PWR_Obj obj, PWR_AttrType type );

void  printAllAttr( PWR_Obj obj )
{
    int i, num = PWR_ObjGetNumAttrs( obj );
    for ( i = 0; i < num; i++ ) {
        PWR_AttrType type; 
        PWR_ObjGetAttrTypeByIndx( obj, i, &type  );
        printObjAttr( obj, type );
    }
}
static void  printObjAttr( PWR_Obj obj, PWR_AttrType type )
{
#if 0
    PWR_Time timeStamp; 
    int intValue[3];
    PWR_AttrUnits scaleValue;
    float floatValue[3];
    #define STRLEN 100
    char stringValue[ STRLEN ], possible[ STRLEN ];
    PWR_AttrDataType vtype;
    PWR_ObjAttrGetValueType( obj, type, &vtype ); 

    printf("    Attr `%s` vtype=%s ",
         PWR_AttrGetTypeString( type ), attrValueType(vtype));
    switch ( vtype ) {
      case PWR_ATTR_DATA_FLOAT:

        PWR_ObjAttrGetUnits( obj, type, &scaleValue );
        PWR_ObjAttrGetRange( obj, type, &floatValue[0], &floatValue[1]);
        PWR_ObjAttrGetValue( obj, type, &floatValue[2], &timeStamp ); 

        printf("scale=%s min=%f max=%f value=%f timeStamp=%llu\n", 
            attrUnit( scaleValue ), floatValue[0],
            floatValue[1], floatValue[2], timeStamp );
        break;

      case PWR_ATTR_DATA_INT:
        PWR_ObjAttrGetUnits( obj, type, &scaleValue  );
        PWR_ObjAttrGetRange( obj, type, &intValue[0], &intValue[1] );
        PWR_ObjAttrGetValue( obj, type, &intValue[2], &timeStamp ); 

        time_t tmp;
        PWR_TimeConvert( timeStamp, &tmp );
        printf("scale=%s min=%i max=%i value=%i timeStamp=%s\n",
            attrUnit( scaleValue ), intValue[0], intValue[1],
            intValue[2], ctime( & tmp )  );
        break;

      case PWR_ATTR_DATA_STRING:
    
        PWR_ObjAttrStringGetPossible( obj, type, possible, 100 ),
        PWR_ObjAttrStringGetValue( obj, type, stringValue, 100, &timeStamp );
        printf("possible=`%s` value=`%s`\n", possible, stringValue );
        break;
    }
#endif
}

const char* getObjName( PWR_Obj obj )
{
    return PWR_ObjGetName( obj );
}

const char * attrValueType( PWR_AttrDataType type )
{
    switch( type ) {
      case PWR_ATTR_DATA_FLOAT:
        return "float";
      case PWR_ATTR_DATA_INT:
        return "int";
      case PWR_ATTR_DATA_STRING:
        return "string";
    }
    return "";
}

const char * attrUnit( PWR_AttrUnits scale )
{
    switch( scale ) {
      case PWR_ATTR_UNITS_1:
        return "1";
      case PWR_ATTR_UNITS_KILO:
        return "Kilo";
      case PWR_ATTR_UNITS_MEGA:
        return "Mega";
      case PWR_ATTR_UNITS_GIGA:
        return "Giga";
      case PWR_ATTR_UNITS_TERA:
        return "Tera";
      case PWR_ATTR_UNITS_PETA:
        return "Peta";
      case PWR_ATTR_UNITS_EXA:
        return "Exa";
    }
    return "";
}

