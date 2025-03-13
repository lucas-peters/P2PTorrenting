import { useEffect, useState } from "react";
import axios from "axios";
import "./App.css";
import { ClientNode } from "./ClientNode";
import { BootstrapNode } from "./BootstrapNode";

export const BASE_URL = "http://localhost:8010/proxy";

function App() {
  const [runningContainers, setRunningContainers] = useState([]);

  const [newBootstrapId, setNewBootstrapId] = useState("");
  const [newBootstrapIp, setNewBootstrapIp] = useState("");

  const [newClientId, setNewClientId] = useState("");
  const [newClientIp, setNewClientIp] = useState("");
  const [newClientApiPort, setNewClientApiPort] = useState("");

  const [retryGetContainer, setRetryGetContainer] = useState(0);

  useEffect(() => {
    (async () => {
      let response = await axios.get(BASE_URL + `/containers/json`).catch();
      setRunningContainers(response.data);
    })();
  }, [retryGetContainer]);

  const clientNodes = runningContainers.filter(
    (container) => container.Image === "my-client-image"
  );

  const bootstrapNodes = runningContainers.filter(
    (container) => container.Image === "my-bootstrap-image"
  );

  const addBootstrapNode = async () => {
    await axios.post(
      BASE_URL + `/containers/create`,
      {
        ...getBootstrapConfig({ ip: newBootstrapIp, name: newBootstrapId }),
      },
      {
        params: {
          name: newBootstrapId,
        },
      }
    );
    await axios
      .post(BASE_URL + `/containers/${newBootstrapId}/start`)
      .then(() => {
        setNewBootstrapId("");
        setNewBootstrapIp("");
        setRetryGetContainer(retryGetContainer + 1); // refresh the page
      })
      .catch((err) => console.log(err));
  };

  const addClientNode = async () => {
    await axios.post(
      BASE_URL + `/containers/create`,
      {
        ...getClientConfig({ ip: newClientIp, apiPort: newClientApiPort }),
      },
      {
        params: {
          name: newClientId,
        },
      }
    );
    await axios
      .post(BASE_URL + `/containers/${newClientId}/start`)
      .then(() => {
        setNewClientId("");
        setNewClientIp("");
        setRetryGetContainer(retryGetContainer + 1);
      })
      .catch((err) => console.log(err));
  };

  return (
    <div className="App">
      <h1>Torrent System</h1>
      <div
        style={{
          display: "flex",
          flexDirection: "row",
          justifyContent: "space-between",
          border: "2px solid black",
          padding: 16,
          margin: 16,
        }}
      >
        <div
          style={{
            display: "flex",
            flexDirection: "column",
            flex: 1,
            padding: 8,
            margin: 8,
            border: "1px solid black",
          }}
        >
          <h2>Bootstrap Nodes</h2>
          {bootstrapNodes.map((container) => (
            <BootstrapNode
              key={container["Id"]}
              container={container}
              setRetryGetContainer={setRetryGetContainer}
            />
          ))}
          <div style={{ flex: 1 }} />
          <div style={{ display: "flex", flexDirection: "row", gap: 8 }}>
            <input
              value={newBootstrapId}
              placeholder="New Bootstrap ID"
              onChange={(e) => setNewBootstrapId(e.target.value)}
              type="text"
            />
            <input
              placeholder="New Bootstrap IP"
              value={newBootstrapIp}
              onChange={(e) => setNewBootstrapIp(e.target.value)}
              type="text"
            />
            <button
              disabled={!newBootstrapId || !newBootstrapIp}
              onClick={() => addBootstrapNode()}
            >
              Add Bootstrap Node
            </button>
          </div>
        </div>
        <div
          style={{ flex: 1, padding: 8, margin: 8, border: "1px solid black" }}
        >
          <h2>Client Nodes</h2>
          {clientNodes.map((container) => (
            <ClientNode
              key={container["Id"]}
              container={container}
              setRetryGetContainer={setRetryGetContainer}
            />
          ))}
          <div style={{ flex: 1 }} />
          <div style={{ display: "flex", flexDirection: "row", gap: 8 }}>
            <input
              value={newClientId}
              placeholder="New Client ID"
              onChange={(e) => setNewClientId(e.target.value)}
              type="text"
            />
            <input
              placeholder="New Client IP"
              value={newClientIp}
              onChange={(e) => setNewClientIp(e.target.value)}
              type="text"
            />
            <input
              placeholder="New Client API Port"
              value={newClientApiPort}
              onChange={(e) => setNewClientApiPort(e.target.value)}
              type="text"
            />
            <button
              disabled={!newClientId || !newClientIp}
              onClick={() => addClientNode()}
            >
              Add Client Node
            </button>
          </div>
        </div>
      </div>
    </div>
  );
}

const getBootstrapConfig = ({ ip }) => {
  const containerConfig = {
    Image: "my-bootstrap-image",
    HostConfig: {
      NetworkMode: "p2p-network",
      // PortBindings: {
      //   "6881/tcp": [{ HostPort: "6881" }],
      //   "6881/udp": [{ HostPort: "6881" }],
      //   "7881/tcp": [{ HostPort: "7881" }],
      //   "7881/udp": [{ HostPort: "7881" }],
      // },
    },
    NetworkingConfig: {
      EndpointsConfig: {
        "p2p-network": {
          IPAMConfig: {
            IPv4Address: ip,
          },
        },
      },
    },
    Env: ["GOSSIP_HOST=" + ip],
  };

  return containerConfig;
};

const getClientConfig = ({ ip, apiPort }) => {
  const containerConfig = {
    Image: "my-client-image",
    HostConfig: {
      NetworkMode: "p2p-network",
      PortBindings: {
        // "6881/tcp": [{ HostPort: "6881" }],
        // "6881/udp": [{ HostPort: "6881" }],
        // "7881/tcp": [{ HostPort: "7881" }],
        // "7881/udp": [{ HostPort: "7881" }],
        "8080/tcp": [{ HostPort: apiPort }],
      },
    },
    NetworkingConfig: {
      EndpointsConfig: {
        "p2p-network": {
          IPAMConfig: {
            IPv4Address: ip,
          },
        },
      },
    },
    Env: ["GOSSIP_HOST=" + ip],
  };

  return containerConfig;
};

export default App;
