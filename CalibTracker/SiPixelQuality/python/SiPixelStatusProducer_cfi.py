import FWCore.ParameterSet.Config as cms

process.siPixelStatusProducer = cms.EDProducer("SiPixelStatusProducer",
    SiPixelStatusProducerParameters = cms.PSet(
        badPixelFEDChannelCollections = cms.VInputTag(cms.InputTag('siPixelDigis')),
        pixelClusterLabel = cms.untracked.InputTag("siPixelClusters::RECO"),
        monitorOnDoubleColumn = cms.untracked.bool(False),
        resetEveryNLumi = cms.untracked.int32( 1 )
    )
)

