"""Defines the schema classes used to describe the Live API and its functionality."""
from collections import namedtuple
from collections import OrderedDict
from enum import Enum
from json import dumps
from json import JSONDecoder
from json import JSONEncoder
import numbers
import re
from .result import Result
from six import string_types


class Schema(object):
    """Class that describes a type, function, callback, or input/output argument.

    Schemas provide all of the type information required by the rpc mechanism, in fact the api is fully generated just
    from information in these schemas - as such they constitute a custom interface definition language (IDL).

    Members:
        type < Schema.Type >: The type being represented by the schema (see Schema.Type).
        role < Schema.Role >: The role of the schema within the api (See Schema.Role).
        detail < int (16-bit) >: Reserved for use in api generation.
        count < int >: Used when the type is Type.EArray to indicate the length of the fixed size array.
        type_name <string > Used when role is Role.EFunction or Role.ECallback to name the function or callback.
            Also used if role is Role.EValue if the schema represents a named type (i.e. a class). In all of these cases
            the type will be Type.ENamedTuple. Finally this is also used if the schema type is Type.ERef. in this case,
            the schema is incomplete and should be subtituted for a schema of the same type_name within the
            SchemaRegistry
        enum_values < [(string, int)] >: If type is Type.EEnum* then this member is a list of name-value tuples
            providing every legal name and value for the enum.
        sub_schemas < [(string, Schema)] >: a list of name-Schema pairs if the types requires sub schemas.
            In many cases the name fields may be empty. The use of sub_schemas depends on the Schema type:
            - If type is Type.EArray then sub_schemas has one entry: [("", <Schema for type in array>)]
            - If type is Type.EList then sub_schemas has one entry: [("", <Schema for type in list>)]
            - If type is Type.ETuple then sub_schemas has one entry for each tuple element:
                  [("", <Schema for 1st element in tuple>), ("", <Schema for 2nd element in tuple>)...]
            - If type is Type.ENamedTuple then sub_schemas has one entry for each tuple element:
                  [("1stElementName",<Schema for 1st element in tuple>), ("2ndElementName",<Schema for 2nd element in tuple>)...]
        self.docs < [string] >: Optional. If non-empty, the first doc string in the list is the documentation for this schema.
            If the schema type is Type.ENamedTuple then subsequent strings document each class member or function or callback
            input/output. If the schema type is Type.EEnum* then each subsequent string provides the documentation for each
            enum value in the order they are listed in enum_values
    """
    class Type(Enum):
        """Possible Schema Types:

        EnumValues:
            EVoid: Invalid Schema.
            EBool: A boolean value - 8-bit value 0 or 1 in binary, "true" or "false" in JSON, True or False in python.
            EInt8: An integer - encoded as an 8-bit value in binary, and as an integer in JSON and python.
            EInt16: An integer - encoded as a 16-bit value in binary, and as an integer in JSON and python.
            EInt32: An integer - encoded as a 32-bit value in binary, and as an integer in JSON and python.
            EInt64: An integer - encoded as a 64-bit value in binary, and as an integer in JSON and python.
            EUInt8: An unsigned integer - encoded as an 8-bit value in binary, and as an integer in JSON and python.
            EUInt16: An unsigned integer - encoded as an 16-bit value in binary, and as an integer in JSON and python.
            EUInt32: An unsigned integer - encoded as an 32-bit value in binary, and as an integer in JSON and python.
            EFloat32: A floating point value - encoded as a 32-bit value in binary, and a float in JSON and python.
            EFloat64: A floating point value - encoded as a 64-bit value in binary, and a float in JSON and python.
            EString: A string - 32bit length followed by UTF8 characters in binary, a quoted and escaped string in JSON and a string in python.
            EArray: A fixed size array - concatenated values in binary, an array in JSON, and a tuple in python.
            ETuple: A tuple of values - concatenated values in binary, an array in JSON, and tuple in python.
            ENamedTuple: As ETuple but values typically represent named members of a class or named arguments to a function or callback.
                         These are encoded identically to ETuple in binary. In JSON they are usually encoded as ETuple (values from the
                         server are encoded this way) but can also be serialised as a JSON object (the server will accept classes, but not
                         currently functions encoded as JSON objects). In python these are generally encoded to and from classes.
            EEnum8: An enum - encoded as an 8-bit value in binary, a quoted value name in JSON and an Enum in python.
            EEnum16: An enum - encoded as an 16-bit value in binary, a quoted value name in JSON and an Enum in python.
            EEnum32: An enum - encoded as a 32-bit value in binary, a quoted value name in JSON and an Enum in python.
            ERef: An incomplete schema that should be substituted with elements from a complete version of the schema with
                  the same type_name. In particular the type, enum_values, sub_schemas and docs members will require substitution.
                  The ERef type can reduce duplication when describing nested schemas and also allows for the representation of recursive
                  types such as Schema itself.
        """
        EVoid = 0
        EBool = 1
        EInt8 = 2
        EInt16 = 3
        EInt32 = 4
        EInt64 = 5
        EUInt8 = 6
        EUInt16 = 7
        EUInt32 = 8
        EUInt64 = 9
        EFloat32 = 10
        EFloat64 = 11
        EString = 12
        EArray = 13
        EList = 14
        ETuple = 15
        ENamedTuple = 16
        EEnum8 = 17
        EEnum16 = 18
        EEnum32 = 19
        ERef = 20

    class Role(Enum):
        """Possible roles that a Schema can have.

        EnumValues:
            EValue: The schema represents a value
            EFunction: The schema represents a function - it must be of type Type.ENamedTuple
            ECallback: The schema represents a callback - it must be of type Type.ENamedTuple
            EInput: The schema represents an input - this must be a sub_schema of a Role.EFunction or Role.ECallback Schema
            EInputOutput: The schema represents both an input/output argument (currently unused)
            EOutput: The schema represents an output - this must be a sub_schema of a Role.EFunction
            EReturn: Similar to EOutput, but in languages like C++, specifies that the output if the return value
                     rather than an output parameter.
            EResult: Similar to EReturn, but specified that the value should be interpreted as a result code
                     (see result.Result). The Schema must be of type Type.EUInt32.
        """
        EValue = 0
        EFunction = 1
        ECallback = 2
        EInput = 3
        EInputOutput = 4
        EOutput = 5
        EReturn = 6
        EResult = 7

    def __init__(self, type=Type.EVoid, type_name="", enum_values=[]):
        self.type = type
        self.role = self.Role.EValue
        self.detail = 0
        self.count = 0
        self.type_name = type_name
        self.enum_values = enum_values
        self.sub_schemas = []
        self.docs = []

    def __eq__(self, other):
        return self.__dict__ == other.__dict__

    @classmethod
    def make_result(cls):
        """Helper function to programatically create a Schema representing a result code."""
        schema = cls(cls.Type.EInt32)
        schema.role = cls.Role.EResult
        return schema

    @classmethod
    def make_list(cls, sub_schema):
        """Helper function to programatically create a Schema representing a list of some sub_schema type."""
        schema = cls(cls.Type.EList, "")
        schema.sub_schemas = [("", sub_schema)]
        return schema

    @classmethod
    def make_tuple(cls, *sub_schemas):
        """Helper function to programatically create a Schema representing a type of some sub_schema types."""
        schema = cls(cls.Type.ETuple, "")
        schema.sub_schemas = [("", sub_schema) for sub_schema in sub_schemas]
        return schema

    @classmethod
    def make_ref(cls, name):
        """Helper function to programatically create a ref Schema."""
        return cls(cls.Type.ERef, name)

    @classmethod
    def make_function(cls, function_name, return_schema):
        """Helper function to programatically create a function Schema with some return value schema."""
        schema = cls(cls.Type.ENamedTuple, function_name)
        schema.role = cls.Role.EFunction
        schema.sub_schemas = [("Return", return_schema)]
        if schema.sub_schemas[-1][1].role != cls.Role.EResult:
            schema.sub_schemas[-1][1].role = cls.Role.EReturn
        return schema

    @classmethod
    def make_class(cls, name):
        """Helper function to programatically create a Schema for a named class."""
        schema = cls(cls.Type.ENamedTuple, name)
        return schema

    def add_input(self, name, schema):
        """Helper function to programatically add an input Schema to a function or callback Schema."""
        self.sub_schemas.append((name, schema))
        self.sub_schemas[-1][1].role = self.Role.EInput
        return self

    def add_output(self, name, schema):
        """Helper function to programatically add an output Schema to a function Schema."""
        self.sub_schemas.append((name, schema))
        self.sub_schemas[-1][1].role = self.Role.EOutput
        return self

    def add_member(self, name, schema):
        """Helper function to programatically add a member Schema to a class Schema."""
        self.sub_schemas.append((name, schema))
        return self


class SchemaServices:
    """This class provides a central registry for Schemas and performs serialisation of python objects to and from JSON."""

    class JSONMode(Enum):
        """Possible modes when serialising python object to JSON.

        EnumValues:
            Friendly: Classes are encoded as JSON objects i.e. {"Member1Name":Member1Value,"Member2Name":Member2Value...}
            FriendlyNoDefaults: As Friendly except that any class members that have their default value are omitted.
            Small: This is the default mode for communication with the server. Classes are just encoded as JSON arrays
                   i.e. [Member1Value,Member2Value]
        """
        Friendly = 0
        FriendlyNoDefaults = 1
        Small = 2

    SchemaRecord = namedtuple("SchemaRecord", ["schema", "pythonic_names", "object"])
    schema_records = {}

    @classmethod
    def register_schema(cls, class_type, schema):
        """Register a schema with SchemaServices."""
        if schema.type_name == "":
            return
        if schema.type_name in cls.schema_records:
            if cls.schema_records[schema.type_name].schema != schema:
                raise RuntimeError("Conflicting schemas detected for " + schema.type_name)
            return
        # workaround for missing __qualname__ in python 2.7
        if schema.role == Schema.Role.EValue:
            class_type._registered_type_name = schema.type_name
        name_pairs = []
        if schema.type == Schema.Type.ENamedTuple:
            for sub_schema in schema.sub_schemas:
                pythonic_name = re.sub('(.)([A-Z][a-z]+)', r'\1_\2', sub_schema[0])
                pythonic_name = re.sub('([a-z0-9])([A-Z])', r'\1_\2', pythonic_name).lower()
                name_pairs.append((sub_schema[0], pythonic_name))
        cls.schema_records[schema.type_name] = cls.SchemaRecord(schema, OrderedDict(name_pairs), class_type)

    @classmethod
    def register_json_schema(cls, class_type, json_schema):
        """Register a schema with SchemaServices by providing its JSON representation."""
        cls.register_schema(class_type, cls.read(json_schema, cls.schema_records["Schema"].schema))

    @classmethod
    def deregister_schema(cls, schema_type_name):
        """Deregister schema from SchemaServices."""
        cls.schema_records.pop(schema_type_name)

    @classmethod
    def schema_record(cls, type_name):
        """ Returns the schema record for the named type or function"""
        return cls.schema_records.get(type_name, None)

    @classmethod
    def schema(cls, type_name):
        """ Returns the schema record for the named type or function"""
        record = cls.schema_records.get(type_name, None)
        if record:
            return record.schema
        return None

    @classmethod
    def interface_schemas(cls, interface_name):
        """ Returns the complete list of schemas required by the specified class interface"""

        def recurse_schemas(schema, schema_dict):
            """Recursively add all sub-schemas to dictionary ordered by the schema type name."""
            if schema.type_name != "":
                if schema.type == Schema.Type.ERef:
                    record = cls.schema_record(schema.type_name)
                    if record is None:
                        raise RuntimeError("Interface uses unknown schema " + schema.type_name)
                    schema = record.schema
            for sub_schema_entry in schema.sub_schemas:
                recurse_schemas(sub_schema_entry[1], schema_dict)
            schema_dict[schema.type_name] = schema

        interface_schemas = [named_record[1].schema for named_record in cls.schema_records.items() if named_record[0].startswith(interface_name + ".")]
        schema_dict = OrderedDict()
        for schema in interface_schemas:
            recurse_schemas(schema, schema_dict)
        return [entry[1] for entry in schema_dict.items()]

    @classmethod
    def write(cls, obj, mode=JSONMode.Small, pretty=False):
        """Serialise a python object to JSON"""
        def encoder(obj):
            """JSON encoder implementation."""
            if isinstance(obj, Enum):
                return obj.name[1:]
            if isinstance(obj, Result):
                return obj.code
            json_writable_value = None
            if hasattr(obj, "_registered_type_name"):
                record = cls.schema_records.get(obj._registered_type_name, None)
                if record:
                    if mode == cls.JSONMode.Small:
                        json_writable_value = [getattr(obj, x[1]) for x in record.pythonic_names.items()]
                    elif mode == cls.JSONMode.Friendly:
                        json_writable_value = OrderedDict([(x[0], getattr(obj, x[1])) for x in record.pythonic_names.iteritems()])
                    elif mode == cls.JSONMode.FriendlyNoDefaults:
                        default = obj.__class__()
                        json_writable_value = OrderedDict([(x[0], getattr(obj, x[1])) for x in record.pythonic_names.iteritems() if getattr(obj, x[1]) != getattr(default, x[1])])
                if json_writable_value:
                    return json_writable_value
            return JSONEncoder.default(cls, obj)

        if pretty:
            return dumps(obj, default=encoder, indent=4, separators=(',', ': '), ensure_ascii=False)
        return dumps(obj, default=encoder, ensure_ascii=False)

    @classmethod
    def read(cls, input_string, schema, index=0):
        """Deserialise an input JSONString to a python object"""
        decoder = JSONDecoder()
        decoded, index = decoder.raw_decode(input_string, index)

        def decode(input, schema):
            # special case for handling result codes
            if schema.role == Schema.Role.EResult:
                if not isinstance(input, int):
                    raise ValueError("Could not interpret " + str(input) + " as a result code")
                return Result(input)

            # special case for handling functions. note that only outputs (and return values) are read
            if schema.role == Schema.Role.EFunction:
                output_schemas = [s[1] for s in schema.sub_schemas if (s[1].role is Schema.Role.EReturn) or (s[1].role is Schema.Role.EOutput)]
                return tuple(decode(element[0], element[1]) for element in zip(input, output_schemas))

            # special case for handling callbacks.
            if schema.role == Schema.Role.ECallback:
                if len(input) != len(schema.sub_schemas):
                    raise ValueError("Could not interpret " + str(input) + " as callback parameter tuple of length " + str(len(schema.sub_schemas)))
                return tuple(decode(element[0], element[1][1]) for element in zip(input, schema.sub_schemas))

            if schema.type == Schema.Type.EVoid:
                return None

            if schema.type == Schema.Type.EBool:
                if isinstance(input, bool):
                    return input
                if isinstance(input, int):
                    return False if input == 0 else True
                raise ValueError("Could not interpret " + str(input) + " as " + schema.type.name)

            if schema.type.value >= Schema.Type.EInt8.value and schema.type.value <= Schema.Type.EFloat64.value:
                if not isinstance(input, numbers.Number):
                    raise ValueError("Could not interpret " + str(input) + " as " + schema.type.name)
                return input

            if schema.type == Schema.Type.EString:
                if not isinstance(input, string_types):
                    raise ValueError("Could not interpret " + str(input) + " as " + schema.type.name)
                return input

            if schema.type == Schema.Type.EArray:
                if len(input) != schema.count:
                    raise ValueError("Could not interpret " + str(input) + " as array of length " + schema.count)
                return tuple(decode(element, schema.sub_schemas[0][1]) for element in input)

            if schema.type == Schema.Type.EList:
                return [decode(element, schema.sub_schemas[0][1]) for element in input]

            if schema.type == Schema.Type.ETuple:
                if len(input) != len(schema.sub_schemas):
                    raise ValueError("Could not interpret " + str(input) + " as tuple of length " + str(len(schema.sub_schemas)))
                return tuple(decode(element[0], element[1][1]) for element in zip(input, schema.sub_schemas))

            if schema.type == Schema.Type.ENamedTuple:
                record = cls.schema_records.get(schema.type_name, None)
                if not record:
                    raise ValueError("Could not interpret unknown type " + schema.type_name)
                output = record.object()
                if isinstance(input, dict):
                    for names, sub_schema in zip(record.pythonic_names.items(), schema.sub_schemas):
                        input_element = input.get(names[0], None)
                        if input_element:
                            setattr(output, names[1], decode(input_element, sub_schema[1]))
                else:
                    if len(input) != len(record.pythonic_names):
                        raise ValueError("Could not interpret " + str(input) + " as class with " + str(len(record.pythonic_names)) + " members")
                    for input_element, names, sub_schema in zip(input, record.pythonic_names.items(), schema.sub_schemas):
                        setattr(output, names[1], decode(input_element, sub_schema[1]))
                return output

            if schema.type.value >= Schema.Type.EEnum8.value and schema.type.value <= Schema.Type.EEnum32.value:
                record = cls.schema_records.get(schema.type_name, None)
                if not record:
                    raise ValueError("Could not read unregistered enum type " + str(input))
                return record.object["E" + input]

            if schema.type == Schema.Type.ERef:
                return decode(input, cls.schema_records[schema.type_name].schema)

        return decode(decoded, schema)

# pre-register schemas for Schema class
SchemaServices.register_schema(Schema, Schema.make_class("Schema")
                               .add_member("Type", Schema.make_ref("Schema.Type"))
                               .add_member("Role", Schema.make_ref("Schema.Role"))
                               .add_member("Detail", Schema(Schema.Type.EUInt16))
                               .add_member("Count", Schema(Schema.Type.EUInt32))
                               .add_member("TypeName", Schema(Schema.Type.EString))
                               .add_member("EnumValues", Schema.make_list(Schema.make_tuple(Schema(Schema.Type.EString), Schema(Schema.Type.EInt32))))
                               .add_member("SubSchemas", Schema.make_list(Schema.make_tuple(Schema(Schema.Type.EString), Schema(Schema.Type.ERef, "Schema"))))
                               .add_member("Docs", Schema.make_list(Schema(Schema.Type.EString))))
SchemaServices.register_schema(Schema.Type, Schema(Schema.Type.EEnum8, "Schema.Type", [(e.name, e.value) for e in Schema.Type]))
SchemaServices.register_schema(Schema.Role, Schema(Schema.Type.EEnum8, "Schema.Role", [(e.name, e.value) for e in Schema.Role]))
