// Copyright Epic Games, Inc. All Rights Reserved.

using MongoDB.Bson.Serialization;
using MongoDB.Bson.Serialization.Serializers;
using MongoDB.Driver;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Linq.Expressions;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Threading.Tasks;
using System.Transactions;

namespace EpicGames.MongoDB
{
	/// <summary>
	/// Allows building a transactional MongoDB update of several fields, and applying those to an in-memory object. Useful for transactional updates.
	/// </summary>
	/// <typeparam name="TDocument">The type of document to update</typeparam>
	public class TransactionBuilder<TDocument> where TDocument : class
	{
		/// <summary>
		/// Information about an update to a field
		/// </summary>
		class FieldUpdate
		{
			/// <summary>
			/// The update definition
			/// </summary>
			public UpdateDefinition<TDocument> Update;

			/// <summary>
			/// Applies an update to the given object
			/// </summary>
			public Action<object> Apply;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="Update">The update definition</param>
			/// <param name="Apply">Applies the update to a given object</param>
			public FieldUpdate(UpdateDefinition<TDocument> Update, Action<object> Apply)
			{
				this.Update = Update;
				this.Apply = Apply;
			}
		}

		/// <summary>
		/// Helper class for determining the indexer for a particular type
		/// </summary>
		/// <typeparam name="T">The type to find the indexer for</typeparam>
		static class Reflection<T>
		{
			/// <summary>
			/// Caches the indexer for this type
			/// </summary>
			public static PropertyInfo Indexer { get; } = GetIndexerProperty();

			/// <summary>
			/// Finds the indexer property from a type
			/// </summary>
			/// <returns>The property info</returns>
			static PropertyInfo GetIndexerProperty()
			{
				foreach (PropertyInfo Property in typeof(T).GetProperties())
				{
					if (Property.GetIndexParameters().Length > 0)
					{
						return Property;
					}
				}
				return null!;
			}
		}

		/// <summary>
		/// List of field updates
		/// </summary>
		List<FieldUpdate> FieldUpdates = new List<FieldUpdate>();

		/// <summary>
		/// Whether the transaction is currenty empty
		/// </summary>
		public bool IsEmpty
		{
			get { return FieldUpdates.Count == 0; }
		}

		/// <summary>
		/// Adds a setting to this transaction
		/// </summary>
		/// <typeparam name="TField">Type of the field to be updated</typeparam>
		/// <param name="Expr">The expresion defining the field to update</param>
		/// <param name="Value">New value for the field</param>
		public void Set<TField>(Expression<Func<TDocument, TField>> Expr, TField Value)
		{
			UpdateDefinition<TDocument> Update = Builders<TDocument>.Update.Set(Expr, Value);
			Action<object> Apply = Target => Assign(Target, Expr, Value);
			FieldUpdates.Add(new FieldUpdate(Update, Apply));
		}

		/// <summary>
		/// Adds an update to remove a field
		/// </summary>
		/// <param name="Expr">The expresion defining the field to update</param>
		public void Unset(Expression<Func<TDocument, object>> Expr)
		{
			UpdateDefinition<TDocument> Update = Builders<TDocument>.Update.Unset(Expr);
			Action<object> Apply = Target => Unassign(Target, Expr.Body);
			FieldUpdates.Add(new FieldUpdate(Update, Apply));
		}

		/// <summary>
		/// Updates a property dictionary with a set of adds and removes
		/// </summary>
		/// <param name="Expr">Lambda expression defining a field to update</param>
		/// <param name="Updates">List of updates</param>
		public void UpdateDictionary<TKey, TValue>(Expression<Func<TDocument, Dictionary<TKey, TValue>>> Expr, IEnumerable<KeyValuePair<TKey, TValue?>> Updates) where TKey : class where TValue : class
		{
			PropertyInfo IndexerProperty = Reflection<Dictionary<TKey, TValue>>.Indexer;
			foreach (KeyValuePair<TKey, TValue?> Update in Updates)
			{
				MethodCallExpression Index = Expression.Call(Expr.Body, IndexerProperty.GetMethod!, new[] { Expression.Constant(Update.Key) });
				if (Update.Value == null)
				{
					Expression<Func<TDocument, object>> IndexExpression = Expression.Lambda<Func<TDocument, object>>(Index, Expr.Parameters[0]);
					Unset(IndexExpression);
				}
				else
				{
					Expression<Func<TDocument, TValue>> IndexExpression = Expression.Lambda<Func<TDocument, TValue>>(Index, Expr.Parameters[0]);
					Set(IndexExpression, Update.Value);
				}
			}
		}

		/// <summary>
		/// Applies this transaction to the given update
		/// </summary>
		/// <param name="Target">The object to update</param>
		public void ApplyTo(TDocument Target)
		{
			foreach (FieldUpdate Setting in FieldUpdates)
			{
				Setting.Apply(Target);
			}
		}

		/// <summary>
		/// Assign a value to a field
		/// </summary>
		/// <param name="Target">The target object to be updated</param>
		/// <param name="FieldExpression">Lambda indicating the field to update</param>
		/// <param name="Value">New value for the field</param>
		static void Assign(object? Target, LambdaExpression FieldExpression, object? Value)
		{
			Expression Body = FieldExpression.Body;
			if (Body.NodeType == ExpressionType.MemberAccess)
			{
				MemberExpression MemberExpression = (MemberExpression)Body;
				object? Object = Evaluate(MemberExpression.Expression!, Target);
				PropertyInfo PropertyInfo = (PropertyInfo)MemberExpression.Member;
				PropertyInfo.SetValue(Object, Value);
			}
			else if (Body.NodeType == ExpressionType.ArrayIndex)
			{
				BinaryExpression BinaryExpression = (BinaryExpression)Body;
				System.Collections.IList List = (System.Collections.IList)Evaluate(BinaryExpression.Left, Target)!;
				int Index = (int)Evaluate(BinaryExpression.Right, Target)!;
				List[Index] = Value;
			}
			else if (Body.NodeType == ExpressionType.Call)
			{
				MethodCallExpression CallExpression = (MethodCallExpression)Body;
				System.Collections.IDictionary Dictionary = (System.Collections.IDictionary)Evaluate(CallExpression.Object!, Target)!;
				object? Key = Evaluate(CallExpression.Arguments[0], null);
				Dictionary[Key!] = Value;
			}
			else if (Body.NodeType == ExpressionType.Index)
			{
				IndexExpression IndexExpression = (IndexExpression)Body;
				object? Object = Evaluate(IndexExpression.Object!, Target);

				object?[] Arguments = new object?[IndexExpression.Arguments.Count];
				for (int Idx = 0; Idx < IndexExpression.Arguments.Count; Idx++)
				{
					Arguments[Idx] = Evaluate(IndexExpression.Arguments[Idx], null);
				}

				IndexExpression.Indexer!.SetValue(Object, Value, Arguments);
			}
			else
			{
				throw new NotImplementedException();
			}
		}

		/// <summary>
		/// Removes an entry from a dictionary
		/// </summary>
		/// <param name="Target">The target object to be updated</param>
		/// <param name="Body">Body of the expression indicating the field to update</param>
		static void Unassign(object? Target, Expression Body)
		{
			if (Body.NodeType == ExpressionType.MemberAccess)
			{
				MemberExpression MemberExpression = (MemberExpression)Body;
				switch (MemberExpression.Member.MemberType)
				{
					case MemberTypes.Field:
						((FieldInfo)MemberExpression.Member).SetValue(Target, null);
						break;
					case MemberTypes.Property:
						((PropertyInfo)MemberExpression.Member).SetValue(Target, null);
						break;
					default:
						throw new NotImplementedException();
				}
			}
			else if (Body.NodeType == ExpressionType.Call)
			{
				MethodCallExpression CallExpression = (MethodCallExpression)Body;
				System.Collections.IDictionary? Dictionary = (System.Collections.IDictionary?)Evaluate(CallExpression.Object!, Target);
				object? Key = Evaluate(CallExpression.Arguments[0], null);
				Dictionary!.Remove(Key!);
			}
			else if (Body.NodeType == ExpressionType.Index)
			{
				IndexExpression IndexExpression = (IndexExpression)Body;
				System.Collections.IDictionary? Dictionary = (System.Collections.IDictionary?)Evaluate(IndexExpression.Object!, Target);
				object? Key = Evaluate(IndexExpression.Arguments[0], null);
				Dictionary!.Remove(Key!);
			}
			else if (Body.NodeType == ExpressionType.Convert)
			{
				UnaryExpression UnaryExpression = (UnaryExpression)Body;
				Unassign(Target, UnaryExpression.Operand);
			}
			else
			{
				throw new NotImplementedException();
			}
		}

		/// <summary>
		/// Evaluates an expression
		/// </summary>
		/// <param name="Expression">The expression to evaluate</param>
		/// <param name="Parameter">Parameter to the unary lambda expression</param>
		/// <returns>Value of the expression</returns>
		static object? Evaluate(Expression Expression, object? Parameter)
		{
			if (Expression.NodeType == ExpressionType.Call)
			{
				MethodCallExpression CallExpression = (MethodCallExpression)Expression;
				object? Object = Evaluate(CallExpression.Object!, Parameter);
				object?[] Arguments = CallExpression.Arguments.Select(x => Evaluate(x, Parameter)).ToArray();
				return CallExpression.Method.Invoke(Object, Arguments);
			}
			else if (Expression.NodeType == ExpressionType.Constant)
			{
				ConstantExpression ConstantExpression = (ConstantExpression)Expression;
				return ConstantExpression.Value;
			}
			else if (Expression.NodeType == ExpressionType.MemberAccess)
			{
				MemberExpression MemberExpression = (MemberExpression)Expression;
				object? Target = Evaluate(MemberExpression.Expression!, Parameter);

				MemberInfo Member = MemberExpression.Member;
				switch (Member.MemberType)
				{
					case MemberTypes.Property:
						return ((PropertyInfo)Member).GetValue(Target);
					case MemberTypes.Field:
						return ((FieldInfo)Member).GetValue(Target);
					default:
						throw new NotImplementedException("Unsupported expression type");
				}
			}
			else if (Expression.NodeType == ExpressionType.ArrayIndex)
			{
				BinaryExpression BinaryExpression = (BinaryExpression)Expression;
				System.Collections.IList List = (System.Collections.IList)Evaluate(BinaryExpression.Left, Parameter)!;
				int Index = (int)Evaluate(BinaryExpression.Right, null)!;
				return List[Index];
			}
			else if (Expression.NodeType == ExpressionType.Parameter)
			{
				return Parameter;
			}
			else
			{
				throw new NotImplementedException();
			}
		}

		/// <summary>
		/// Converts this transaction to an update definition
		/// </summary>
		public UpdateDefinition<TDocument> ToUpdateDefinition()
		{
			return Builders<TDocument>.Update.Combine(FieldUpdates.Select(x => x.Update));
		}
	}
}
