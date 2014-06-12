--
-- Copyright (c) 2012 Citrix Systems, Inc.
-- 
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
-- 
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
-- 
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
--

{-# LANGUAGE GADTs, Arrows #-}
module Protocol
       (
         Application (..)
       , Resource (..)
       , Launcher (..)
       , Credentials (..)
       , Req
       , PNConfig
       , URLString

       , doRequest
       , pnConfigRequest
       , appRequest
       , launcherRequest
       )
       where

import Control.Applicative
import Data.Maybe
import Data.List
import Text.XML.HXT.Core
import Curl

data PNConfig = PNConfig {
    pnEnumerationPath :: String
  , pnResourcePath :: String
  } deriving Show

data Credentials = Credentials { strCredentials :: String }

newtype Xml
      = Xml { strXml :: String }
      deriving Show

data Application = Application {
    appFName :: String
  , appInName :: Resource
  , appDescription :: String
  , appClientType :: String
  , appIsDesktop :: Bool
  } deriving Show

newtype Resource
      = Resource { strResource :: String }
      deriving Show

newtype Launcher
      = Launcher { strLauncher :: String }
      deriving Show

data XmlResponse
   = XmlResponse { respXml :: Xml }

type XmlBuilder  = LA () XmlTree
type XmlParser a = IOSArrow XmlTree a

data Req a where
  XmlPostReq :: URLString -> XmlBuilder -> XmlParser a -> Req a
  StrPostReq :: URLString -> XmlBuilder -> (String -> Maybe a) -> Req a
  XmlGetReq  :: URLString -> XmlParser a -> Req a

xmlParseOpts
  = [ withValidate no
    , withCheckNamespaces no
    , withSubstDTDEntities no ]

pnConfigRequest :: URLString -> Req PNConfig
pnConfigRequest url = XmlGetReq url pnConfigA

appRequest :: PNConfig -> Credentials -> Req [Application]
appRequest cfg c = XmlPostReq (pnEnumerationPath cfg) body (listA appA) where
    body =
      mkelem "NFuseProtocol" [ sattr "version" "5.5" ]
      [ selem "RequestAppData"
        [ mkelem "Scope" [ sattr "traverse" "subtree" ] []
        , selem "DesiredDetails" [ txt "defaults" ]
        , selem "ServerType" [ txt "all" ]
        , selem "ClientType" [ txt "all" ]
        , selem "Credentials" [ constA (strCredentials c) >>> xread ]
        ]
      ]

launcherRequest :: PNConfig -> Credentials -> Resource -> Req Launcher
launcherRequest cfg c r = StrPostReq (pnResourcePath cfg) body (Just . Launcher) where
    body =
      mkelem "PNAgentResourceRequest" []
      [ selem "Resource" [ txt (strResource r) ]
      , selem "ClientType" [ txt "ica30" ]
      , mkelem "ICA_Options" []
        [ selem "ICA_TemplateFile" [ txt "default.ica" ] ]
      , selem "Credentials" [ constA (strCredentials c) >>> xread ]
      ]

pnConfigA :: ArrowXml a => a XmlTree PNConfig
pnConfigA = proc x -> do
      e <- enumerationA -< x
      r <- resourceA -< x
      returnA -< PNConfig e r
  where
    enumerationA, resourceA :: ArrowXml a => a XmlTree String
    enumerationA = getChildren /> hasName "Request" /> hasName "Enumeration" /> hasName "Location" /> getText
    resourceA    = getChildren /> hasName "Request" /> hasName "Resource"    /> hasName "Location" /> getText

appA :: ArrowXml a => a XmlTree Application
appA = deep (hasName "AppData") >>> proc x -> do
  name       <- deep (hasName "FName") /> getText -< x
  inname     <- deep (hasName "InName") /> getText -< x
  clienttype <- deep (hasName "ClientType") /> getText -< x
  desktop    <- (settings >>> hasAttrValue "appisdesktop" (== "true") >>> constA True)
                `orElse` constA False -< x
  desc       <- (settings  /> hasName "Description" /> getText)
                `orElse` constA "" -< x
  returnA -< Application
    { appFName = name
    , appInName = Resource inname
    , appDescription = desc
    , appClientType = clienttype
    , appIsDesktop = desktop }
  where
    settings = deep (hasName "Details") /> hasName "Settings"

https :: URLString -> URLString
https url | "http://"  `isPrefixOf` url = "https://" ++ drop 7 url
          | "https://" `isPrefixOf` url = url
          | otherwise = "https://" ++ url

doRequest :: Req a -> IO (Either CurlError a)
doRequest (XmlGetReq url p)
  = shortcircuit (runParser_ p . Xml) =<< httpGet (https url)
doRequest (XmlPostReq url body p)
  = shortcircuit (runParser_ p . Xml) =<< httpPost (https url) (strXml $ serialise body)
doRequest (StrPostReq url body p)
  = return . parse =<< httpPost (https url) (strXml $ serialise body) where
      parse (Left err) = Left err
      parse (Right  v) = Right $ fromMaybe (error "string parse error") (p v)

shortcircuit f e@(Left err) = return (Left err)
shortcircuit f (Right  x) = Right <$> f x

serialise :: XmlBuilder -> Xml
serialise body
  = Xml (f strs)
  where
    strs    = runLA (root [] [body] >>> writeDocumentToString [withOutputEncoding utf8, withXmlPi yes]) ()
    f []    = error "xml serialise error"
    f (x:_) = x

runParser_ p xml = fromMaybe (error "xml parse error") <$> runParser p xml

runParser :: XmlParser a -> Xml -> IO (Maybe a)
runParser p (Xml xmlstr)
  = return . rv =<< runX (constA xmlstr >>> readFromString xmlParseOpts >>> p) where
    rv [] = Nothing
    rv (h:_) = Just h

xmlExtract :: XmlParser Xml
xmlExtract = writeDocumentToString [ withOutputEncoding utf8, withXmlPi yes ] >>> arr Xml
