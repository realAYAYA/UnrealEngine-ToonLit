# Dynamo DB for Jupiter

Jupiter is a globally replicated service for caching of artifacts used in our build system. These artifacts are stored in S3 but we keep a index of which artifacts exists in a Dynamo DB. 


# Tables
We generally use global tables to allow for replication, we do not seperate into tenent tables, instead this is part of the key (namespace).

# Table Layout

We can use Bucket as a partition key as its intended to be the same for a chunk of objects. Id could then be a sort key. Alternativley the partition key could be a clustered key of the bucket and the name called Id (we append the two strings together to form a ID field). 

| Name      | Type                                          |
| ---       | ---                                           |
| ID        | String                                        | Partition key
| Namespace | String                                        |
| Name      | String                                        |
| Bucket    | String                                        |
| Blobs     | Array of Sha1                                 |
| Metadata  | Opaque json object. Serialized as string      |
| Hash      | Sha1 as string                                |
| LastAccess| DateTime of when this record was las accessed |

# Queries

## GET Record
Most of the traffic will just be requests for fetch or check for existence of a records with a given name and bucket.

## PUT Record
A record can get replaced by another record, but we will generally not update existing records (except for last access, see below). If a existing record is modified we consider it a new record (for most intents and purposes its different).

## Delete Record
This operation will most be invoked as a result of the garbage collection query, while possible for a user to drop a individual record we expect them to most often drop a whole bucket if even deleting anything at all. 

## Drop all records in Bucket
We give users a option to drop all records in a bucket. If name is a sortkey we are unable to do a query for all buckets (a query for partition keys without a sortkey) which prevents us partitioning like that.

Scan table to get back all the values of name given bucket, and then manually delete each item. 


## Last Access updates
Whenever the service adds new records, or fetches old ones, it tracks a list of all the rows it has accessed. This list is then used at certain intervals to update the LastAccess timestamp of each record in the list.

This can use the same partition key as the is normal.

## Garbage Collection
At certain points the table will be garbage collected, with the goal of removing every record that has a last access time older then a cutoff (a week).

This will need a secondary index of LastAccess were just index LastAccess, but if we have a composite key as primary key then we need a composite key for last access as well. As we are unaware of every bucket that could exist we can not partition the cleanup index on that. Thus we intend to add a random number as parition key and join on these when requesting all keys older then a given date.

# Index

## Name (Primary)
| Name      | Type          |
| ---       | ---           |
| ID        | Partition Key |

## Last Access (Secondary)

# GSI

| Name        |  Type           |
| ---         | ---             |
| RandomKey   | Partition Key   | This should scale with the number of writes we do. 1 - 10 should be sufficent. Single key should never reach 1000 writes per sec. Lean towards more then you need.
| LastAccess  | Sort Key        | (should be epoch string)

# Bucket Table

This could also be a sparse object in the main table

| Name        |  Type           |
| ---         | ---             |
| Namespace   | Partition Key   | 
| Bucket      | Sort Key        |

Lambda that reads the writes from stream and creates tenant_bucket list. 
eventually consistent reads
if not exist
update bucket list with a new bucket for the tentant



# Limitations to consider
Dynamo has a max limit of 400kb per row. This means that metadata (as the only truly dynamic field) needs to be capped to make sure the full row does not exceed this limit.
