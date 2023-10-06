using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
    public class StreamMetaData
    {
        public StreamMetaData()
        {
            Stream = null;
            Parent = null;
            Type = StreamType.Development;
            ParentType = StreamType.Development; 
            FirmerThanParent = false;
            ChangeFlowsToParent = false;
            ChangeFlowsFromParent = false;
            IntegToParent = false;
            IntegToParentHow = IntegAction.None;
            ToResult = string.Empty;
            IntegFromParent = false;
            IntegFromParentHow = IntegAction.None;
            FromResult = string.Empty;
        }
        public StreamMetaData(DepotPath stream,
            DepotPath parent,
            StreamType type,
            StreamType parenttype,
            bool firmerthanParent,
            bool changeflowstoparent,
            bool changeflowsfromparent,
            bool integtoparent,
            IntegAction integtoparenthow,
            string toresult,
            bool integfromparent,
            IntegAction integfromparenthow,
            string fromresult
            )
        {
        Stream=stream;
        Parent = parent;
        Type = type;
        ParentType = parenttype;
        FirmerThanParent = firmerthanParent;
        ChangeFlowsToParent = changeflowstoparent;
        ChangeFlowsFromParent = changeflowsfromparent;
        IntegToParent = IntegToParent;
        IntegToParentHow = IntegToParentHow;
        ToResult = toresult;
        IntegFromParent = integfromparent;
        IntegFromParentHow = integfromparenthow;
        FromResult = fromresult;
        }

        public DepotPath Stream { get; set; }
        public DepotPath Parent { get; set; }
        private StringEnum<StreamType> _type;
        public StreamType Type
        {
            get { return _type; }
            set { _type = value; }
        }
        private StringEnum<StreamType> _parenttype;
        public StreamType ParentType
        {
            get { return _parenttype; }
            set { _parenttype = value; }
        }
        public bool FirmerThanParent { get; set; }
        public bool ChangeFlowsToParent { get; set; }
        public bool ChangeFlowsFromParent { get; set; }
        public bool IntegToParent { get; set; }
        private StringEnum<IntegAction> _integtoparenthow = IntegAction.None;
        public IntegAction IntegToParentHow
        {
            get { return (_integtoparenthow == null) ? IntegAction.None : (IntegAction)_integtoparenthow; }
            set { _integtoparenthow = value; }
        }        public string ToResult { get; set; }
        public bool IntegFromParent { get; set; }
        private StringEnum<IntegAction> _integfromparenthow = IntegAction.None;
        public IntegAction IntegFromParentHow
        {
            get { return (_integfromparenthow == null) ? IntegAction.None : (IntegAction)_integfromparenthow; }
            set { _integfromparenthow = value; }
        }
        public string FromResult { get; set; }


        public enum IntegAction
	   {
		/// <summary>
		/// No options.
		/// </summary>
		None		= 0x0000,
		/// <summary>
		/// Leaves all files writable on the client;
		/// by default, only files opened by 'p4 edit'
		/// are writable. If set, files might be clobbered
		/// as a result of ignoring the clobber option.
		/// </summary>
		Merge	= 0x0001,
		/// <summary>
		/// Permits 'p4 sync' to overwrite writable
		/// files on the client.  noclobber is ignored if
		/// allwrite is set.
		/// </summary>
		Copy		= 0x0002
        }

        public void FromIstatCmdTaggedData(TaggedObject obj)
        {
            if (obj.ContainsKey("stream"))
            {
                string p = PathSpec.UnescapePath(obj["stream"]);
                Stream = new DepotPath(p);
            }
            if (obj.ContainsKey("parent"))
            {
                string p = PathSpec.UnescapePath(obj["parent"]);
                Parent = new DepotPath(p);
            }
            if (obj.ContainsKey("type"))
            {
                _type = (obj["type"]);
            }
            if (obj.ContainsKey("parentType"))
            {
                _parenttype = (obj["parentType"]);

            }
            if (obj.ContainsKey("firmerThanParent"))
            {
                bool value;
                bool.TryParse(obj["firmerThanParent"],out value);
                FirmerThanParent = value;
            }
            if (obj.ContainsKey("changeFlowsToParent"))
            {
                bool value;
                bool.TryParse(obj["changeFlowsToParent"], out value);
                ChangeFlowsToParent = value;

            }
            if (obj.ContainsKey("changeFlowsFromParent"))
            {
                bool value;
                bool.TryParse(obj["changeFlowsFromParent"], out value);
                ChangeFlowsFromParent = value;

            }
            if (obj.ContainsKey("integToParent"))
            {
                bool value;
                bool.TryParse(obj["integToParent"], out value);
                IntegToParent = value;

            }
            if (obj.ContainsKey("integToParentHow"))
            {
                _integtoparenthow = (obj["integToParentHow"]);
            }
            if (obj.ContainsKey("toResult"))
            {
                ToResult = obj["toResult"];
            }
            if (obj.ContainsKey("integFromParent"))
            {
                bool value;
                bool.TryParse(obj["integFromParent"], out value);
                IntegFromParent = value;

            }
            if (obj.ContainsKey("integFromParentHow"))
            {
                _integfromparenthow = (obj["integFromParentHow"]);
            }
            if (obj.ContainsKey("fromResult"))
            {
                FromResult = obj["fromResult"];
            }
        }
    }
}
