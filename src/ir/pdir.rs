
pub type IndexType = i32;

pub struct PDExpression {
    // TODO: Fill
}

pub enum PDStatement {
    Assignment,
    Call,
    Intrinsic,
    FlowTransfer,
}

pub enum BlockEdge {
    NoTransfer,
    Fallthrough(IndexType),
    Unconditional(IndexType),
    BinaryCondition(IndexType, IndexType),
    MultiwayCondition(Vec<(u64, IndexType)>),
}

pub enum EdgeIterator<'a> {
    StaticEdgeCount([IndexType; 2], usize, usize),
    VariableEdgeCount(std::slice::Iter<'a, (u64, IndexType)>),
}

impl<'a> Iterator for EdgeIterator<'a> {
    type Item = IndexType;

    fn next(&mut self) -> Option<Self::Item> {
        match self {
            EdgeIterator::StaticEdgeCount(arr, idx, cap) => {
                if *idx < *cap {
                    *idx += 1;
                    Some(arr[*idx - 1])
                } else {
                    None
                }},
            EdgeIterator::VariableEdgeCount(v) => v.next().map(|p| p.1),
        }
    }
}

impl<'a> IntoIterator for &'a BlockEdge {
    type Item = IndexType;
    type IntoIter = EdgeIterator<'a>;

    fn into_iter(self) -> Self::IntoIter {
        match self {
            BlockEdge::NoTransfer => EdgeIterator::StaticEdgeCount([0, 0], 0, 0),
            BlockEdge::Fallthrough(i) => EdgeIterator::StaticEdgeCount([*i, 0], 0, 1),
            BlockEdge::Unconditional(i) => EdgeIterator::StaticEdgeCount([*i, 0], 0, 1),
            BlockEdge::BinaryCondition(i, j) => EdgeIterator::StaticEdgeCount([*i, *j], 0, 2),
            BlockEdge::MultiwayCondition(v) => EdgeIterator::VariableEdgeCount(v.iter()),
        }
    }
}

pub struct BasicBlock {
    pub statements: Vec<PDStatement>,
    pub transfer_cond: Option<PDExpression>,
    pub outedges: BlockEdge,
    pub inedges: BlockEdge,
}

pub struct PDRoutine {
    pub blocks: Vec<BasicBlock>,
}

pub fn test(edge: &mut BlockEdge) {
    for i in edge.into_iter() {
        println!("{}",i);
    }
}
