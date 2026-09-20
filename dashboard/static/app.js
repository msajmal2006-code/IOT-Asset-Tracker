// =========================================
// HEAVY MOTION ALERT
// =========================================

const HEAVY_MOTION_THRESHOLD = 1.50;


// =========================================
// HEAVY MOTION DETECTION
// =========================================

function isHeavyMotion(d)
{
    const ax = Number(d.ax);
    const ay = Number(d.ay);
    const az = Number(d.az);


    if (
        Number.isNaN(ax) ||
        Number.isNaN(ay) ||
        Number.isNaN(az)
    )
    {
        return false;
    }


    const magnitude =
        Math.sqrt(
            (ax * ax) +
            (ay * ay) +
            (az * az)
        );


    return magnitude >= HEAVY_MOTION_THRESHOLD;
}


// =========================================
// CLOCK
// =========================================

function updateClock()
{
    const now = new Date();

    const dateText =
        now.toLocaleDateString(
            "en-IN",
            {
                day: "2-digit",
                month: "short",
                year: "numeric"
            }
        );

    const timeText =
        now.toLocaleTimeString(
            "en-IN",
            {
                hour: "2-digit",
                minute: "2-digit",
                second: "2-digit"
            }
        );

    document.getElementById("date").textContent =
        dateText;

    document.getElementById("clock").textContent =
        timeText;
}


setInterval(
    updateClock,
    1000
);

updateClock();


// =========================================
// LATEST TELEMETRY
// =========================================

async function getLatest()
{
    try
    {
        const response =
            await fetch("/api/latest");


        if (!response.ok)
        {
            throw new Error("Backend error");
        }


        const d =
            await response.json();


        if (!d || !d.asset_id)
        {
            document.getElementById("status").textContent =
                "Waiting for telemetry";

            document.querySelector(".status-dot").style.background =
                "#ffb000";

            return;
        }


        // ---------------------------------
        // CONNECTION STATUS
        // ---------------------------------

        document.getElementById("status").textContent =
            "Live telemetry received";

        document.querySelector(".status-dot").style.background =
            "#25c76a";


        // ---------------------------------
        // ASSET
        // ---------------------------------

        document.getElementById("asset").textContent =
            d.asset_id ?? "--";


        // ---------------------------------
        // EVENT
        // ---------------------------------

        document.getElementById("event").textContent =
            d.event ?? "--";


        // ---------------------------------
        // GPS FIX
        // ---------------------------------

        document.getElementById("fix").textContent =
            Number(d.fix) === 1
                ? "YES"
                : "NO";


        // ---------------------------------
        // DHT11
        // ---------------------------------

        document.getElementById("temp").textContent =
            `${d.temp ?? "--"} °C`;

        document.getElementById("hum").textContent =
            `${d.hum ?? "--"} %`;


        // ---------------------------------
        // GPS
        // ---------------------------------

        document.getElementById("lat").textContent =
            d.lat ?? "--";

        document.getElementById("lon").textContent =
            d.lon ?? "--";

        document.getElementById("speed").textContent =
            d.speed ?? "--";

        document.getElementById("course").textContent =
            d.course ?? "--";


        // ---------------------------------
        // MPU6500
        // ---------------------------------

        document.getElementById("ax").textContent =
            `${d.ax ?? "--"} g`;

        document.getElementById("ay").textContent =
            `${d.ay ?? "--"} g`;

        document.getElementById("az").textContent =
            `${d.az ?? "--"} g`;
    }

    catch (error)
    {
        console.error(
            "Latest telemetry error:",
            error
        );


        document.getElementById("status").textContent =
            "Backend connection error";


        document.querySelector(".status-dot").style.background =
            "#e53935";
    }
}


// =========================================
// HISTORY
// =========================================

async function getHistory()
{
    try
    {
        const response =
            await fetch("/api/history?limit=20");


        if (!response.ok)
        {
            throw new Error("History request failed");
        }


        const data =
            await response.json();


        const table =
            document.getElementById("history");


        const emptyMessage =
            document.getElementById("emptyMessage");


        table.innerHTML = "";


        if (!data || data.length === 0)
        {
            emptyMessage.style.display =
                "flex";

            return;
        }


        emptyMessage.style.display =
            "none";


        data.forEach(d =>
        {
            const row =
                document.createElement("tr");


            // ---------------------------------
            // RECEIVED TIME
            // ---------------------------------

            const receivedTime =
                d.received_at
                    ? new Date(d.received_at)
                        .toLocaleString("en-IN")
                    : "--";


            // ---------------------------------
            // HEAVY MOTION CHECK
            // ---------------------------------

            const heavyMotion =
                d.event === "HEAVY_MOTION" ||
                isHeavyMotion(d);


            let statusHTML;


            if (heavyMotion)
            {
                statusHTML =
                    `
                    <span
                        class="history-alert-dot"
                        title="HIGH PRIORITY: Heavy movement detected">
                        ●
                    </span>
                    `;

                row.classList.add(
                    "heavy-motion-row"
                );
            }
            else
            {
                statusHTML =
                    `
                    <span
                        class="history-normal-dot"
                        title="Normal movement">
                        ●
                    </span>
                    `;
            }


            // ---------------------------------
            // TABLE ROW
            // ---------------------------------

            row.innerHTML = `

                <td class="history-status">
                    ${statusHTML}
                </td>

                <td>
                    ${receivedTime}
                </td>

                <td>
                    ${d.asset_id ?? "--"}
                </td>

                <td>
                    ${d.event ?? "--"}
                </td>

                <td>
                    ${d.temp ?? "--"}
                </td>

                <td>
                    ${d.hum ?? "--"}
                </td>

                <td>
                    ${d.lat ?? "--"}
                </td>

                <td>
                    ${d.lon ?? "--"}
                </td>

                <td>
                    ${d.speed ?? "--"}
                </td>

                <td>
                    ${d.course ?? "--"}
                </td>

            `;


            table.appendChild(row);
        });
    }

    catch (error)
    {
        console.error(
            "History error:",
            error
        );
    }
}


// =========================================
// UPDATE DASHBOARD
// =========================================

async function updateDashboard()
{
    await getLatest();

    await getHistory();
}


// =========================================
// INITIAL LOAD
// =========================================

updateDashboard();


// =========================================
// AUTOMATIC REFRESH
// =========================================

setInterval(
    updateDashboard,
    2000
);